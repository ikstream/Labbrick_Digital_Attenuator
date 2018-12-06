#!/usr/bin/env bash

set -o pipefail -o noclobber -o nounset

# output options
ERROR="$(tput bold; tput setaf 1)[Error]:$(tput sgr0)"
WARNING="$(tput bold ; tput setaf 3)[Warning]:$(tput sgr0)"
INFO="$(tput bold ; tput setaf 2) [INFO]:$(tput sgr0)"

SUCCESS=0
FAILURE=1

VERBOSE=n

LOGPATH="con-test_logs"

# Test if getopt works as expected
#
test_get_opt()
{
	! getopt --test > /dev/null 
	if [[ ${PIPESTATUS[0]} -ne 4 ]]; then
	    return FAILURE
	fi
}

# check if all dependencies are installed
#
check_dependecies()
{
	printf "${INFO} Checking dependencies on $HOSTNAME\n"
	test_get_opt
	check_ret_val "$?" "`getopt --test` failed in this environment."
	hash sshpass 2>/dev/null
	check_ret_val "$?" "sshpass is not installed"
	printf "${INFO} All checks passed on $HOSTNAME\n"
}

# check dependencies on wifi nodes
#
check_remote_deps()
{
	printf "${INFO} Checking dependencies on ${SERVER_IP}"

	printf "${INFO} Checking dependencies on ${CLIENT_IP}"
}

# start iperf server
#
# Arguments:
# u_name - username on the device running the iperf server
# ip - ip of device assigned to be iperf server
#
start_iperf_server()
{
	run="$1"
	log_name="con-test-server-run-${run}-$(date +%F).log"

	iperf_pid=$(sshpass -p "$SERVER_PASSWORD" ssh ${SERVER_USER}@${SERVER_IP} "ps" | awk '/[i]perf/{ print $1 }')
	if [ -z "$iperf_pid" ]; then
		ret=$(sshpass -p "$SERVER_PASSWORD" ssh ${SERVER_USER}@${SERVER_IP} "kill -9 $iperf_pid")
		check_ret_val "$?" "Could not kill iperf: $iperf_pid - $ret"
	fi

	ret=$(sshpass -p "$SERVER_PASSWORD" ssh ${SERVER_USER}@${SERVER_IP} "iperf -s" >> ${LOG_PATH}/${log_name})
	check_ret_val $? "Failed to start iperf server on ${SERVER_IP}: $ret"
}

# start iperf client
#
# Arguments:
# u_name - username on the device running the iperf server
# ip - ip of device assigned to be iperf server
# remote_ip - ip of iperf server
#
start_iperf_client()
{
	run=$1
	log_name="con-test-client-run-${run}-$(date +%F).log"

	iperf_pid=$(sshpass -p "$CLIENT_PASSWORD" ssh ${CLIENT_USER}@${CLIENT_IP} "ps" | awk '/[i]perf/{ print $1 }')
	if [ -z "$iperf_pid" ]; then
		ret=$(sshpass -p "$CLIENT_PASSWORD" ssh ${CLIENT_USER}@${CLIENT_IP} "kill -9 $iperf_pid")
		check_ret_val "$?" "Could not kill iperf: $iperf_pid - $ret"
	fi

	ret=$(sshpass - p "$CLIENT_PASSWORD" ssh ${CLIENT_USER}@${CLIENT_IP} "iperf -c $SERVER_WIFI_IP" >> ${LOG_PATH}/${log_name})
	check_ret_val $? "Failed to start iperf client on ${CLIENT_IP}: $ret"
}

# start attenuators, get the attenuation up
#
# Arguments:
# params - parameters for the attenuator programm
#
start_antennuator()
{
	params="$@"
	/usr/local/bin/attenuator_lab_brick $params
}

# copy package to device and update it
#
# Arguments:
# u_name - user name on remote host
# ip - update package on this host
# pkg_name - name of package to be updated
# path - path to package on current host include package name
#
update_package()
{
	u_name="$1"
	ip="$2"
	path="$3"

	if [ ! -f "$path" ]; then
		printf "${ERROR} ${path} does not exist or cannot be accessed"
		return $FAILURE
	fi

	pkg_name=$(basename "$path")
	scp ${path} ${u_name}@${ip}:/tmp/
	check_ret_val $? "Failed to copy $pkg_name to $ip"
	ret=$(ssh ${u_name}@${ip} "opkg remove --force-depends $pkg_name" 1>/dev/null 2>&1)
	check_ret_val $? "ssh on $ip returned $?: $ret"
	ret=$(ssh ${u_name}@${ip} "opkg install $pkg_name" 1>/dev/null 2>&1)
	check_ret_val $? "ssh on $ip returned $?: $ret"

	return $SUCCESS
}

# print information about this script
#
call_help()
{
	printf "con-test help:\n\n"
	printf "\t -c, --config:\t provide path to conn-test.conf, default ./conn-test.conf \n\n"
	printf "\t -h, --help:\t call this overview\n\n"
	printf "\t -l, --logfile\t path to log file to store output, default ./conn-test.log \n\n"
	printf "\t -v, --verbose\t output more information during a run\n"
}

# Check if value equals zero
# exits with FAILURE on mismatch
#
# Arguments:
# val - value to compare
# string - output string on mismatch
#
check_ret_val()
{
	val="$1"
	string="$2"
	if [[ ! "$val" == "0" ]]; then
		printf "$ERROR $string\n"
		exit $FAILURE
	fi
}

# main function
#
# Arguments:
# args - command line arguments
#
main()
{
	args="$@"
	options=c:hl:v
	loptions=config:,help,logfile:,verbose
	
	config_path="con-test.conf"
	LOG_PATH="con-test_logs"
	
	check_dependecies
	! parsed=$(getopt --options=$options --longoptions=$loptions --name "$0" -- "$args")
	if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
	    exit $FAILURE
	fi
	eval set -- "$parsed"

	while true; do
		case "$1" in
			-c | --config)
				config_path=$2
				shift 2
				;;
			-h | --help)
				call_help
				exit $SUCCESS
				;;
			-l | --logfile)
				LOG_PATH=$2
				shift 2
				;;
			-v | --verbose)
				VERBOSE=y
				shift
				;;
			--)
				break
				;;
			*)
				break
				;;
		esac
	done

	printf "${INFO} Starting conn-test script\n"
	printf "\t\tUsing ${config_path}\n"
	printf "\t\tUsing ${LOG_PATH}/\n"
	if [ ! -f "$config_path" ]; then
		printf "${ERROR} ${config_path} does not exist, or can not be accessed"
		exit $FAILURE
	fi
	source ${config_path}

	mkdir -p $LOG_PATH

	for i in $(seq 1 ${NR_RUNS}); do
		printf "${INFO} Starting test run $i with parameters: ${ATTENUATOR_PARAMS[${i} - 1]}\n"

		#start measurement
		start_iperf_server "$i"
		start_iperf_client "$i"

		start_antennuator ${ATTENUATOR_PARAMS[${i} - 1]}
		if [ -z "$update_pkg$i" ]; then
			update_package "$SERVER_USER" "$SERVER_IP" "${UPDATE_PKG[$i - 1]}"
			update_package "$CLIENT_USER" "$CLIENT_IP" "${UPDATE_PKG[$i - 1]}"
		fi
	done
}

main "$@"
