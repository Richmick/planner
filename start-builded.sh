#!/bin/bash

cd "/app/build/service-executables"
"./planner-gateway" &
"./planner-frontend" &
"./planner-users" &
"./planner-scheduler" &
wait
echo "all services exited"
