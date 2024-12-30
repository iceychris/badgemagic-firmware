#!/bin/bash

# build container
docker build --platform linux/amd64 --build-arg USER_UID=$(id -u) --build-arg USER_GID=$(id -g) -t badgemagic-builder .

# start container if not already running
if ! docker ps | grep -w "badgemagic-builder" > /dev/null 2>&1; then
    docker run -d --platform linux/amd64 --rm -v $(pwd):/home/builder/workspace badgemagic-builder /bin/sleep infinity
fi

# get container id
CONTAINER_ID=$(docker ps | grep badgemagic-builder | awk '{print $1}')

# build firmware 
docker exec -it $CONTAINER_ID make && wchisp flash build/badgemagic-ch582.elf
