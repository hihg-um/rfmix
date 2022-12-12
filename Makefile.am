AUTOMAKE_OPTIONS = foreign
CFLAGS += -ggdb -Wall -march=core2
CXXFLAGS += -ggdb -Wall -march=core2
LDFLAGS += -lpthread

bin_PROGRAMS = rfmix simulate
rfmix_SOURCES = cmdline-utils.c inputline.cpp genetic-map.cpp md5rng.cpp hash-table.cpp mm.cpp rfmix.cpp load-input.cpp random-forest.cpp crf.cpp output.cpp gensamples.cpp s-sample.cpp

simulate_SOURCES = cmdline-utils.c inputline.cpp genetic-map.cpp hash-table.cpp vcf.cpp simulate.cpp s-sample.cpp s-subpop.cpp

OS_BASE ?= ubuntu
OS_VER ?= 22.04

ORG_NAME := hihg-um
PROJECT_NAME ?= rfmuix

USER ?= $(USER)
USERID := `id -u`
USERGNAME ?= ad
USERGID ?= 1533

IMAGE_REPOSITORY =
IMAGE = $(ORG_NAME)/$(USER)/$(PROJECT_NAME):latest

# Use this for debugging builds. Turn off for a more slick build log
DOCKER_BUILD_ARGS := --progress=plain

test_docker:
	@echo "Testing docker image: $(IMAGE)"
	@docker run -it -v /mnt:/mnt $(IMAGE) -v

clean:
	@docker rmi -f --no-prune $(IMAGE)

docker: Dockerfile
	@docker build $(DOCKER_BUILD_ARGS) -t $(IMAGE) \
		--build-arg BASE_IMAGE=$(OS_BASE):$(OS_VER) \
		--build-arg USERNAME=$(USER) \
		--build-arg USERID=$(USERID) \
		--build-arg USERGNAME=$(USERGNAME) \
		--build-arg USERGID=$(USERGID) \
		.
