# demucs-ui-app (raylib GUI). Workers live in git submodules.
# macOS default worker is the native mlx-c binary; the PyInstaller worker is kept as *-py targets.
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO := $(abspath $(ROOT)/..)
MLX := $(REPO)/workers/demucs-mac-silicon
SIBLING_MLX := $(abspath $(REPO)/../demucs-mac-silicon)
C_MLX ?= $(if $(wildcard $(SIBLING_MLX)/worker-c/CMakeLists.txt),$(SIBLING_MLX),$(MLX))
WINW := $(REPO)/workers/demucs-windows
BUILD := $(ROOT)/build
BIN := $(BUILD)/demucs_mlx_app
PYTHON := $(MLX)/.venv/bin/python
C_PYTHON ?= $(C_MLX)/.venv/bin/python
DIST_WORKER := $(ROOT)/dist/worker
C_WORKER ?= $(DIST_WORKER)/demucs_mlx_worker
C_CACHE ?= $(ROOT)/models
RUN_ENV := DEMUCS_MLX_WORKER="$(C_WORKER)" DEMUCS_MLX_CACHE="$(C_CACHE)"
RUN_PY_ENV := DEMUCS_MLX_PYTHON="$(PYTHON)" DEMUCS_MLX_CACHE="$(ROOT)/models" PYTHONPATH="$(MLX)"

.PHONY: all configure build run run-py run-c process process-py process-c clean bundle bundle-py worker worker-py \
	worker-c worker-torch models models-py check-c verify-mp3 verify-psarc verify-psarc-mac externals version

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

check-c:
	@test -f "$(C_WORKER)" -a -x "$(C_WORKER)" || (echo "ERROR: missing C worker at $(C_WORKER). Run: make -C app worker" >&2; exit 1)
	@test -f "$(dir $(C_WORKER))mlx.metallib" || (echo "ERROR: missing mlx.metallib beside $(C_WORKER)" >&2; exit 1)
	@test -f "$(C_CACHE)/htdemucs_6s.safetensors" || (echo "ERROR: missing $(C_CACHE)/htdemucs_6s.safetensors. Run: make -C app models" >&2; exit 1)
	@test -f "$(C_CACHE)/htdemucs_6s.json" || (echo "ERROR: missing $(C_CACHE)/htdemucs_6s.json. Run: make -C app models" >&2; exit 1)

run: build check-c
	$(RUN_ENV) "$(BIN)" --mp3

run-py: build
	@test -d "$(MLX)/demucs_mlx" || (echo "ERROR: missing workers/demucs-mac-silicon submodule" >&2; exit 1)
	$(RUN_PY_ENV) "$(BIN)"

# Usage: make process FILES="a.mp3 b.psarc" OUT=~/Documents/psarc/stems
process: build check-c
	@test -n "$(FILES)" || (echo "Usage: make process FILES='song.mp3' OUT=~/Documents/psarc/stems"; exit 1)
	$(RUN_ENV) "$(BIN)" $(FILES) --out "$(or $(OUT),$(HOME)/Documents/psarc/stems)" --mp3 --headless

process-py: build
	@test -n "$(FILES)" || (echo "Usage: make process-py FILES='song.mp3' OUT=~/Documents/psarc/stems"; exit 1)
	$(RUN_PY_ENV) "$(BIN)" $(FILES) --out "$(or $(OUT),$(HOME)/Documents/psarc/stems)" --mp3

verify-mp3: build check-c
	@mkdir -p "$(ROOT)/build/verify_out"
	$(RUN_ENV) "$(BIN)" "$(ROOT)/testdata/short_20s.mp3" --out "$(ROOT)/build/verify_out" --mp3 --headless
	@ls -la "$(ROOT)/build/verify_out/short_20s/"
	@test -f "$(ROOT)/build/verify_out/short_20s/guitar.mp3"
	@test -f "$(ROOT)/build/verify_out/short_20s/vocals.mp3"
	@echo "verify-mp3 OK"

verify-psarc: build check-c
	@mkdir -p "$(ROOT)/build/verify_out"
	$(RUN_ENV) "$(BIN)" "$(HOME)/Documents/psarc/CDLC/Parquet-Courts_Freebird-II_v1_p.psarc" \
		--out "$(ROOT)/build/verify_out" --mp3 --headless
	@ls -la "$(ROOT)/build/verify_out/Parquet-Courts_Freebird-II_v1_p/"
	@test -f "$(ROOT)/build/verify_out/Parquet-Courts_Freebird-II_v1_p/guitar.mp3"
	@echo "verify-psarc OK"

verify-psarc-mac: build check-c
	@test -n "$(PSARC_M)" || (echo "Usage: make verify-psarc-mac PSARC_M=/path/song_m.psarc"; exit 1)
	@test -f "$(PSARC_M)" || (echo "missing $(PSARC_M)"; exit 1)
	@mkdir -p "$(ROOT)/build/verify_out"
	@base=$$(basename "$(PSARC_M)" .psarc); \
	$(RUN_ENV) "$(BIN)" "$(PSARC_M)" --out "$(ROOT)/build/verify_out" --mp3 --headless && \
	test -f "$(ROOT)/build/verify_out/$$base/guitar.mp3" && \
	echo "verify-psarc-mac OK ($$base)"

# safetensors + json for the C worker (converts the MLX pickle first if needed).
models:
	@test -f "$(C_MLX)/scripts/export_safetensors.py" || (echo "ERROR: $(C_MLX) has no scripts/export_safetensors.py (git submodule update --init --recursive)" >&2; exit 1)
	@test -x "$(C_PYTHON)" || (echo "ERROR: missing $(C_PYTHON). From $(C_MLX): uv venv && uv pip install -e '.[convert]'" >&2; exit 1)
	@mkdir -p "$(ROOT)/models"
	DEMUCS_MLX_CACHE="$(ROOT)/models" PYTHONPATH="$(C_MLX)" "$(C_PYTHON)" "$(C_MLX)/scripts/export_safetensors.py" -n htdemucs_6s -o "$(ROOT)/models"
	@test -f "$(ROOT)/models/htdemucs_6s.safetensors"
	@test -f "$(ROOT)/models/htdemucs_6s.json"

models-py:
	@test -d "$(MLX)/demucs_mlx" || (echo "ERROR: missing workers/demucs-mac-silicon submodule (git submodule update --init --recursive)" >&2; exit 1)
	@mkdir -p "$(ROOT)/models"
	DEMUCS_MLX_CACHE="$(ROOT)/models" PYTHONPATH="$(MLX)" "$(PYTHON)" -c "from demucs_mlx.model_converter import get_mlx_model; get_mlx_model('htdemucs_6s'); print('ok')"

worker:
	$(MAKE) -C "$(C_MLX)" worker
	rm -rf "$(DIST_WORKER)"
	mkdir -p "$(DIST_WORKER)"
	cp "$(C_MLX)/dist/worker/demucs_mlx_worker" "$(DIST_WORKER)/demucs_mlx_worker"
	cp "$(C_MLX)/dist/worker/mlx.metallib" "$(DIST_WORKER)/mlx.metallib"

worker-py:
	@test -x "$(MLX)/scripts/freeze_worker.sh" || (echo "ERROR: missing workers/demucs-mac-silicon submodule" >&2; exit 1)
	DEMUCS_MLX_PYTHON="$(PYTHON)" bash "$(MLX)/scripts/freeze_worker.sh"
	rm -rf "$(DIST_WORKER)"
	mkdir -p "$(DIST_WORKER)"
	cp -R "$(MLX)/dist/worker/demucs_mlx_worker" "$(DIST_WORKER)/"

worker-torch:
	@test -f "$(WINW)/scripts/freeze_worker_torch.ps1" || (echo "ERROR: missing workers/demucs-windows submodule" >&2; exit 1)
	pwsh -File "$(WINW)/scripts/freeze_worker_torch.ps1"
	mkdir -p "$(ROOT)/dist/worker"
	rm -rf "$(ROOT)/dist/worker/demucs_torch_worker"
	cp -R "$(WINW)/dist/worker/demucs_torch_worker" "$(ROOT)/dist/worker/"

bundle: build worker models
	DMX_WORKER_KIND=c bash "$(ROOT)/scripts/build_app.sh"

bundle-py: build worker-py models-py
	DMX_WORKER_KIND=py bash "$(ROOT)/scripts/build_app.sh"

run-c: run
process-c: process
worker-c: worker

clean:
	rm -rf "$(BUILD)" "$(ROOT)/dist"
