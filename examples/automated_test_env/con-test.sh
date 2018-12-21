#!/usr/bin/env bash

set -o pipefail -o noclobber
set -x

# version numbers
MAJOR=0
MINOR=1
RELEASE=1

# output options
ERROR="$(tput bold; tput setaf 1)[Error]:$(tput sgr0)"
WARNING="$(tput bold ; tput setaf 3)[Warning]:$(tput sgr0)"
INFO="$(tput bold ; tput setaf 2) [INFO]:$(tput sgr0)"

SUCCESS=0
FAILURE=1

VERBOSE=n

LOGPATH="con-test_logs"

SUDO=''

# 0 -> certificate, 1 -> password
S_AUTH=1
C_AUTH=1

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
	if [ -z $SERVER_CERTIFICATE ] || [ -z $CLIENT_CERTIFICATE ]; then
		hash sshpass 2>/dev/null
		if (( $? != 0 )); then
			printf "Please provide client/server certificate, or"
			printf " install sshpass and provide passwords\n"
			exit $FAILURE
		fi

		if [ -z $SERVER_PASSWORD ]; then
			if [ ! -f $SERVER_CERTIFICATE ] ||
			   [ -z $SERVER_CERTIFICATE ]; then
				printf "No server password set, no certificate"
				printf " provided/accessible.\n"
				exit $FAILURE
			fi
			S_AUTH=0
		fi

		if [ -z $CLIENT_PASSWORD ]; then
			if [ ! -f $CLIENT_CERTIFICATE ] ||
			   [ -z $CLIENT_CERTIFICATE ];then
				printf "No client password set, no certificate"
				printf " provided/accessible.\n"
				exit $FAILURE
			fi
			C_AUTH=0
		fi
	fi

	if (( $EUID != 0 )); then
		hash sudo 2>/dev/null
		check_ret_val "$?" "Please run as root, or _install_ sudo."
		SUDO='sudo'
	fi
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
	log_name="con-test-server-run-${run}-$(date +%F-%H-%M-%S).log"

	param="-s"
	if [ -n "${IPERF_PARAM_SERVER[${i} - 1]}" ]; then
		param="${param} ${IPERF_PARAM_SERVER[${i} - 1]}"
	fi

	if (( $S_AUTH == 1 )); then
		iperf_pid=$(sshpass -p "$SERVER_PASSWORD" \
			    ssh ${SERVER_USER}@${SERVER_IP} "ps" \
			    | awk '/[i]perf/{ print $1 }')
		if [ -n "$iperf_pid" ]; then
			ret=$(sshpass -p "$SERVER_PASSWORD" \
			      ssh ${SERVER_USER}@${SERVER_IP} \
			      "kill -9 $iperf_pid")
			check_ret_val "$?" "Could not kill iperf: $iperf_pid - $ret"
		fi

		ret=$(nohup sshpass -p "$SERVER_PASSWORD" \
		      ssh ${SERVER_USER}@${SERVER_IP} \
		      "iperf3 $param" >> ${LOG_PATH}/${log_name} &)
		check_ret_val $? "Failed to start iperf server on ${SERVER_IP}: $ret"
	else
		iperf_pid=$(ssh -i $SERVER_CERTIFICATE \
			    ${SERVER_USER}@${SERVER_IP} "ps" \
			    | awk '/[i]perf/{ print $1 }')
		if [ -n "$iperf_pid" ]; then
			ret=$(ssh -i $SERVER_CERTIFICATE \
			      ${SERVER_USER}@${SERVER_IP} "kill -9 $iperf_pid")
			check_ret_val "$?" "Could not kill iperf: $iperf_pid - $ret"
		fi

		ret=$(ssh -i $SERVER_CERTIFICATE \
		      ${SERVER_USER}@${SERVER_IP} \
		      "iperf3 $param" >> ${LOG_PATH}/${log_name} &)
		check_ret_val $? "Failed to start iperf server on ${SERVER_IP}: $ret"
	fi
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
	log_name="con-test-client-run-${run}-$(date +%F-%H-%M-%S).log"

	param="-c"
	if [ -n "${IPERF_PARAM_CLIENT[${i} - 1]}" ]; then
		param="${IPERF_PARAM_CLIENT[${i} - 1]} $param"
	fi

	if (( $C_AUTH == 1 )); then
		iperf_pid=$(sshpass -p "$CLIENT_PASSWORD" \
			    ssh ${CLIENT_USER}@${CLIENT_IP} "ps" \
			    | awk '/[i]perf/{ print $1 }')
		if [ -n "$iperf_pid" ]; then
			ret=$(sshpass -p "$CLIENT_PASSWORD" \
			      ssh ${CLIENT_USER}@${CLIENT_IP} \
			      "kill -9 $iperf_pid")
			check_ret_val "$?" "Could not kill iperf: $iperf_pid - $ret"
		fi

		ret=$(nohup sshpass -p "$CLIENT_PASSWORD" \
		      ssh ${CLIENT_USER}@${CLIENT_IP} \
		      "iperf3 $param $SERVER_WIFI_IP" \
		      >> ${LOG_PATH}/${log_name} &)
		check_ret_val $? "Failed to start iperf client on ${CLIENT_IP}: $ret"
	else
		iperf_pid=$(ssh -i $CLIENT_CERTIFICATE \
			    ${CLIENT_USER}@${CLIENT_IP} "ps" \
			    | awk '/[i]perf/{ print $1 }')
		if [ -n "$iperf_pid" ]; then
			ret=$(ssh -i $CLIENT_CERTIFICATE \
			      ${CLIENT_USER}@${CLIENT_IP} \
			      "kill -9 $iperf_pid")
			check_ret_val "$?" "Could not kill iperf: $iperf_pid - $ret"
		fi

		ret=$(nohup ssh -i $CLIENT_CERTIFICATE \
		      ${CLIENT_USER}@${CLIENT_IP} \
		      "iperf3 $param $SERVER_WIFI_IP" \
		      >> ${LOG_PATH}/${log_name} &)
		check_ret_val $? "Failed to start iperf client on ${CLIENT_IP}: $ret"
	fi
}

# start attenuators, get the attenuation up
#
# Arguments:
# params - parameters for the attenuator programm
#
start_antennuator()
{
	params="$@"
	$SUDO /usr/local/bin/attenuator_lab_brick $params
	check_ret_val $? "Could not start the attenuation programm"
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
	u_name="${PARAMS[0]}"
	ip="${PARAMS[1]}"
	path="${PARAMS[2]}"
	pw="${PARAMS[3]}"
	key="${PARAMS[4]}"

	if [ ! -f "$path" ]; then
		printf "${ERROR} ${path} does not exist or cannot be accessed"
		return $FAILURE
	fi

	pkg_name=$(basename "$path")
	if [ -n "$key" ]; then
		scp -i $key ${path} ${u_name}@${ip}:/tmp/
		check_ret_val $? "Failed to copy $pkg_name to $ip"

		ssh -i $key ${u_name}@${ip} "opkg update" 1>/dev/null 2>&1

		ret=$(ssh -i $key ${u_name}@${ip} \
		      "opkg remove --force-depends $pkg_name" 1>/dev/null 2>&1)
		check_ret_val $? "ssh on $ip returned $?: Failed to run opkg remove"

		ret=$(ssh -i $key ${u_name}@${ip} \
		      "opkg install --force-reinstall /tmp/${pkg_name}" \
		      1>/dev/null 2>&1)
		check_ret_val $? "ssh on $ip returned $?: Failed to run opkg install"
	elif [ -n "$pw" ]; then
		sshpass -p $pw scp ${path} ${u_name}@${ip}:/tmp/
		check_ret_val $? "Failed to copy $pkg_name to $ip"

		sshpass -p $pw ssh ${u_name}@${ip} "opkg update" \
			1>/dev/null 2>&1

		ret=$(sshpass -p $pw ssh ${u_name}@${ip} \
			"opkg remove --force-depends $pkg_name" \
			1>/dev/null 2>&1)
		check_ret_val $? "ssh on $ip returned $?: Failed to run opkg remove"

		ret=$(sshpass -p $pw ssh ${u_name}@${ip} \
		      "opkg install --force-reinstall /tmp/${pkg_name}" \
		      1>/dev/null 2>&1)
		check_ret_val $? "ssh on $ip returned $?: Failed to run opkg install"
	fi

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
	printf "\t -V, --version\t print con-test version\n"
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

# print version of con-test
#
print_version()
{
	printf "${INFO} You are using Version %d.%d-%d\n" $MAJOR $MINOR $RELEASE
}

# main function
#
# Arguments:
# args - command line arguments
#
main()
{
	args=$@
	options=c:hl:vV
	loptions=config:,help,logfile:,verbose,version
	
	config_path="con-test.conf"
	LOG_PATH="con-test_logs"
	
	! parsed=$(getopt --options=$options --longoptions=$loptions --name "$0" -- $args)
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
			-V | --version)
				print_version
				exit $SUCCESS
				;;
			--)
				shift
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

	check_dependecies

	mkdir -p $LOG_PATH

	for i in $(seq 1 ${NR_RUNS}); do
		printf "${INFO} Starting test run $i with parameters:"
		printf " ${ATTENUATOR_PARAMS[${i} - 1]}\n"

		if [ -n "${UPDATE_PKG[${i} - 1]}" ]; then
			printf "${INFO} update package: ${UPDATE_PKG[${i} - 1]}"

			PARAMS=("$SERVER_USER" "$SERVER_IP" \
				"${UPDATE_PKG[$i - 1]}" "$SERVER_PASSWORD" \
				"$SERVER_CERTIFICATE")
			update_package $PARAMS
			unset $PARAMS

			PARAMS=("$CLIENT_USER" "$CLIENT_IP"
				"${UPDATE_PKG[${i} - 1]}" "$CLIENT_PASSWORD" \
				"$CLIENT_CERTIFICATE")
			update_package $PARAMS
			unset $PARAMS
		fi

		#start measurement
		start_iperf_server "$i"
		start_iperf_client "$i"

		start_antennuator ${ATTENUATOR_PARAMS[${i} - 1]}
	done
}

main $@
