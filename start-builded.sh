#!/bin/bash

cd "/app/build/service-executables"
"./planner-gateway" &
"./planner-users" &
wait
echo "all services exited"
