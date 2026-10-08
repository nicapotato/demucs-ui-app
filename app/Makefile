# demucs-ui-app (raylib GUI). Workers live in git submodules.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO := $(abspath $(ROOT)/..)
MLX := $(REPO)/workers/demucs-mac-silicon
C_MLX ?= $(abspath $(REPO)/../demucs-mac-silicon)
C_WORKER ?= $(C_MLX)/dist/worker/demucs_mlx_worker
C_CACHE ?= $(C_MLX)/models
WINW := $(REPO)/workers/demucs-windows
BUILD := $(ROOT)/build
BIN := $(BUILD)/demucs_mlx_app
PYTHON := $(MLX)/.venv/bin/python
RUN_ENV := DEMUCS_MLX_PYTHON="$(PYTHON)" DEMUCS_MLX_CACHE="$(ROOT)/models" PYTHONPATH="$(MLX)"
RUN_C_ENV := DEMUCS_MLX_WORKER="$(C_WORKER)" DEMUCS_MLX_CACHE="$(C_CACHE)"

.PHONY: all configure build run run-c process-c clean bundle worker worker-c worker-torch models verify-mp3 verify-psarc verify-psarc-mac externals version

all: build

externals:
	bash "$(ROOT)/scripts/setup_externals.sh"

configure: externals
	@mkdir -p "$(BUILD)"
	cd "$(BUILD)" && cmake .. -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0

build: configure
	cmake --build "$(BUILD)" -j
	@echo "Built $(BIN)"

version: build
	"$(BIN)" --version

run: build
	@test -d "$(MLX)/demucs_mlx" || (echo "ERROR: missing workers/demucs-mac-silicon submodule" >&2; exit 1)
	$(RUN_ENV) "$(BIN)"

run-c: build
	@test -x "$(C_WORKER)" || (echo "ERROR: missing C worker at $(C_WORKER). From demucs-mac-silicon: make worker" >&2; exit 1)
	@test -f "$(C_CACHE)/htdemucs_6s.safetensors" || (echo "ERROR: missing $(C_CACHE)/htdemucs_6s.safetensors. From demucs-mac-silicon: make models-export" >&2; exit 1)
	@test -f "$(C_CACHE)/htdemucs_6s.json" || (echo "ERROR: missing $(C_CACHE)/htdemucs_6s.json" >&2; exit 1)
	@test -f "$(dir $(C_WORKER))mlx.metallib" || (echo "ERROR: missing mlx.metallib beside $(C_WORKER)" >&2; exit 1)
	$(RUN_C_ENV) "$(BIN)" --mp3

# Usage: make process-c FILES="a.wav b.psarc" OUT=~/Documents/psarc/stems
process-c: build
	@test -n "$(FILES)" || (echo "Usage: make process-c FILES='song.wav' OUT=~/Documents/psarc/stems"; exit 1)
	@test -x "$(C_WORKER)" || (echo "ERROR: missing C worker at $(C_WORKER)" >&2; exit 1)
	@test -f "$(C_CACHE)/htdemucs_6s.safetensors" || (echo "ERROR: missing $(C_CACHE)/htdemucs_6s.safetensors" >&2; exit 1)
	$(RUN_C_ENV) "$(BIN)" $(FILES) --out "$(or $(OUT),$(HOME)/Documents/psarc/stems)" --mp3 --headless

worker-c:
	$(MAKE) -C "$(C_MLX)" worker
	mkdir -p "$(ROOT)/dist/worker"
	cp "$(C_MLX)/dist/worker/demucs_mlx_worker" "$(ROOT)/dist/worker/demucs_mlx_worker"
	cp "$(C_MLX)/dist/worker/mlx.metallib" "$(ROOT)/dist/worker/mlx.metallib"

# Usage: make process FILES="a.mp3 b.psarc" OUT=~/Documents/psarc/stems
process: build
	@test -n "$(FILES)" || (echo "Usage: make process FILES='song.mp3' OUT=~/Documents/psarc/stems"; exit 1)
	$(RUN_ENV) "$(BIN)" $(FILES) --out "$(or $(OUT),$(HOME)/Documents/psarc/stems)" --mp3

verify-mp3: build
	@mkdir -p "$(ROOT)/build/verify_out"
	$(RUN_ENV) "$(BIN)" "$(ROOT)/testdata/short_20s.mp3" --out "$(ROOT)/build/verify_out" --mp3
	@ls -la "$(ROOT)/build/verify_out/short_20s/"
	@test -f "$(ROOT)/build/verify_out/short_20s/guitar.mp3"
	@test -f "$(ROOT)/build/verify_out/short_20s/vocals.mp3"
	@echo "verify-mp3 OK"

verify-psarc: build
	@mkdir -p "$(ROOT)/build/verify_out"
	$(RUN_ENV) "$(BIN)" "$(HOME)/Documents/psarc/CDLC/Parquet-Courts_Freebird-II_v1_p.psarc" \
		--out "$(ROOT)/build/verify_out" --mp3
	@ls -la "$(ROOT)/build/verify_out/Parquet-Courts_Freebird-II_v1_p/"
	@test -f "$(ROOT)/build/verify_out/Parquet-Courts_Freebird-II_v1_p/guitar.mp3"
	@echo "verify-psarc OK"

verify-psarc-mac: build
	@test -n "$(PSARC_M)" || (echo "Usage: make verify-psarc-mac PSARC_M=/path/song_m.psarc"; exit 1)
	@test -f "$(PSARC_M)" || (echo "missing $(PSARC_M)"; exit 1)
	@mkdir -p "$(ROOT)/build/verify_out"
	@base=$$(basename "$(PSARC_M)" .psarc); \
	$(RUN_ENV) "$(BIN)" "$(PSARC_M)" --out "$(ROOT)/build/verify_out" --mp3 && \
	test -f "$(ROOT)/build/verify_out/$$base/guitar.mp3" && \
	echo "verify-psarc-mac OK ($$base)"

models:
	@test -d "$(MLX)/demucs_mlx" || (echo "ERROR: missing workers/demucs-mac-silicon submodule (git submodule update --init --recursive)" >&2; exit 1)
	@mkdir -p "$(ROOT)/models"
	DEMUCS_MLX_CACHE="$(ROOT)/models" PYTHONPATH="$(MLX)" "$(PYTHON)" -c "from demucs_mlx.model_converter import get_mlx_model; get_mlx_model('htdemucs_6s'); print('ok')"

worker:
	@test -x "$(MLX)/scripts/freeze_worker.sh" || (echo "ERROR: missing workers/demucs-mac-silicon submodule" >&2; exit 1)
	DEMUCS_MLX_PYTHON="$(PYTHON)" bash "$(MLX)/scripts/freeze_worker.sh"
	mkdir -p "$(ROOT)/dist/worker"
	rm -rf "$(ROOT)/dist/worker/demucs_mlx_worker"
	cp -R "$(MLX)/dist/worker/demucs_mlx_worker" "$(ROOT)/dist/worker/"

worker-torch:
	@test -f "$(WINW)/scripts/freeze_worker_torch.ps1" || (echo "ERROR: missing workers/demucs-windows submodule" >&2; exit 1)
	pwsh -File "$(WINW)/scripts/freeze_worker_torch.ps1"
	mkdir -p "$(ROOT)/dist/worker"
	rm -rf "$(ROOT)/dist/worker/demucs_torch_worker"
	cp -R "$(WINW)/dist/worker/demucs_torch_worker" "$(ROOT)/dist/worker/"

bundle: build worker
	bash "$(ROOT)/scripts/build_app.sh"

clean:
	rm -rf "$(BUILD)" "$(ROOT)/dist"
