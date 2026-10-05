#!/bin/bash

cd "/app/build/service-executables"
"./planner-gateway" &
wait
echo "all services exited"
