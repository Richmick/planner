#!/bin/bash

cd "/app/build/service-executables"
"./planner-gateway" &
"./planner-users" &
"./planner-frontend" &
wait
echo "all services exited"
