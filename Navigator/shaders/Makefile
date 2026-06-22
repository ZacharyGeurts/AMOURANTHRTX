# =============================================================================
# AMOURANTHRTX — SPIR-V shader build (glslc)
# Run at the START of ./linux.sh — outputs land in assets/shaders/ and are
# copied next to the binary by CMake copy_assets. Runtime only loads .spv files.
# =============================================================================

PROJECT_ROOT := $(abspath ../..)
BUILD_TYPE   ?= Debug

GLSLC     ?= glslc
SPIRV_VAL ?= spirv-val

INCLUDE   := -I$(CURDIR)/raytracing -I$(CURDIR)/compute
SPIRV_ENV := --target-spv=spv1.6 --target-env=vulkan1.4

ifeq ($(BUILD_TYPE),Release)
  OPT := -O
else
  OPT := -g -O0
endif

OUT_COMPUTE := $(PROJECT_ROOT)/assets/shaders/compute
OUT_RT      := $(PROJECT_ROOT)/assets/shaders/raytracing

COMPUTE_SRCS := $(wildcard compute/*.comp)
COMPUTE_OUTS := $(patsubst compute/%.comp,$(OUT_COMPUTE)/%.spv,$(COMPUTE_SRCS))

RT_SRCS := $(wildcard raytracing/*.rgen raytracing/*.rmiss raytracing/*.rchit \
                       raytracing/*.rahit raytracing/*.rcall)
RT_OUTS := $(foreach s,$(RT_SRCS),$(OUT_RT)/$(basename $(notdir $(s))).spv)

ALL_OUTS := $(COMPUTE_OUTS) $(RT_OUTS)

.PHONY: all clean
all: $(ALL_OUTS)
	@echo "shaders: $(words $(ALL_OUTS)) SPIR-V modules → assets/shaders/"

$(OUT_COMPUTE)/%.spv: compute/%.comp
	@mkdir -p $(OUT_COMPUTE)
	@echo "glslc: $<"
	$(GLSLC) $(INCLUDE) $(if $(filter aos_load CANVAS x86,$*),-g -O0,$(OPT)) $(SPIRV_ENV) \
		-fshader-stage=comp "$<" -o "$@"
	$(SPIRV_VAL) --target-env vulkan1.4 "$@"

# Raytracing rules (glslc infers stage from extension)
$(OUT_RT)/%.spv: raytracing/%.rgen
	@mkdir -p $(OUT_RT)
	@echo "glslc: $<"
	$(GLSLC) $(INCLUDE) $(OPT) $(SPIRV_ENV) "$<" -o "$@"
	$(SPIRV_VAL) --target-env vulkan1.4 "$@"

$(OUT_RT)/%.spv: raytracing/%.rmiss
	@mkdir -p $(OUT_RT)
	@echo "glslc: $<"
	$(GLSLC) $(INCLUDE) $(OPT) $(SPIRV_ENV) "$<" -o "$@"
	$(SPIRV_VAL) --target-env vulkan1.4 "$@"

$(OUT_RT)/%.spv: raytracing/%.rchit
	@mkdir -p $(OUT_RT)
	@echo "glslc: $<"
	$(GLSLC) $(INCLUDE) $(OPT) $(SPIRV_ENV) "$<" -o "$@"
	$(SPIRV_VAL) --target-env vulkan1.4 "$@"

$(OUT_RT)/%.spv: raytracing/%.rahit
	@mkdir -p $(OUT_RT)
	@echo "glslc: $<"
	$(GLSLC) $(INCLUDE) $(OPT) $(SPIRV_ENV) "$<" -o "$@"
	$(SPIRV_VAL) --target-env vulkan1.4 "$@"

$(OUT_RT)/%.spv: raytracing/%.rcall
	@mkdir -p $(OUT_RT)
	@echo "glslc: $<"
	$(GLSLC) $(INCLUDE) $(OPT) $(SPIRV_ENV) "$<" -o "$@"
	$(SPIRV_VAL) --target-env vulkan1.4 "$@"

clean:
	rm -rf "$(OUT_COMPUTE)" "$(OUT_RT)"