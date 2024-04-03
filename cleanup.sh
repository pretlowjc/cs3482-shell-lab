#!/usr/bin/bash

#list of processes to kill if they are running
commands=("tsh" "./tsh" "myint" "./myint" "myspin" "./myspin" \
          "mysplit" "./mysplit" "mystop" "./mystop" "tshref" "./tshref" \
          "/usr/bin/perl" )


ps x | while IFS= read -r line; do
    if [[ $line =~ ^[[:blank:]]*([[:digit:]]+)[[:blank:]]+[^[:blank:]]+[[:blank:]]+[^[:blank:]]+[[:blank:]]+[^[:blank:]]+[[:blank:]]+([^[:blank:]]+) ]]; then
       proc_pid="${BASH_REMATCH[1]}"
       process="${BASH_REMATCH[2]}"
       for comnd in ${commands[@]}; do
           if [[ $comnd == $process ]]; then 
               echo "killing $process with pid $proc_pid"
               kill -9 $proc_pid
           fi
       done
    fi
done 
