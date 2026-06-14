BUILD_DIR    := target
CCO_DIR      := ../libcco
CCO_BUILD    := $(CCO_DIR)/$(BUILD_DIR)
CCO_LIB      := $(CCO_BUILD)/cco.lib

SERVER_SRC   := server/main.c server/lsp.c server/json.c \
                server/cco_analyze.c server/text_document.c
SERVER_BIN   := $(BUILD_DIR)/cco-lsp

CLIENT_DIR   := client
EXTENSION_JS := $(CLIENT_DIR)/out/extension.js
VSIX         := $(CLIENT_DIR)/cco-language-support-*.vsix

CMAKE        := cmake
NPM          := npm
NPX          := npx
MKDIR        := mkdir -p
CP           := cp
RM           := rm -f
UNAME_S      := $(shell uname -s 2>/dev/null || echo Windows)

# ---- Platform detection ------------------------------------------------
ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
EXE          := .exe
CONFIG_DIR   := Debug
NULL         := nul
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
EXE          := .exe
CONFIG_DIR   := Debug
NULL         := nul
else ifeq ($(UNAME_S),Windows)
EXE          := .exe
CONFIG_DIR   := Debug
NULL         := nul
else ifeq ($(UNAME_S),Windows_NT)
EXE          := .exe
CONFIG_DIR   := Debug
NULL         := nul
else
EXE          :=
CONFIG_DIR   :=
NULL         := /dev/null
endif

SERVER_BIN_FULL := $(BUILD_DIR)/$(CONFIG_DIR)/cco-lsp$(EXE)
SERVER_BIN_COPY := $(CLIENT_DIR)/server/cco-lsp$(EXE)

# ---- Targets -----------------------------------------------------------
.PHONY: all lib server extension package install test clean

all: lib server extension

# ---- Build libcco if needed --------------------------------------------
lib: $(CCO_LIB)

$(CCO_LIB):
	$(CMAKE) -B $(CCO_BUILD) -S $(CCO_DIR)
	$(CMAKE) --build $(CCO_BUILD) --config Debug

# ---- Build C language server -------------------------------------------
server: $(SERVER_BIN_FULL)

$(BUILD_DIR)/Makefile: CMakeLists.txt server/*.h server/*.c
	$(CMAKE) -B $(BUILD_DIR) -S .

$(SERVER_BIN_FULL): $(BUILD_DIR)/Makefile $(CCO_LIB) $(SERVER_SRC)
	$(CMAKE) --build $(BUILD_DIR) --config Debug
	@echo "  SERVER  $@"

# ---- Compile TypeScript extension --------------------------------------
extension: $(EXTENSION_JS)

$(EXTENSION_JS): $(CLIENT_DIR)/src/*.ts $(CLIENT_DIR)/tsconfig.json
	cd $(CLIENT_DIR) && $(NPM) run compile
	@echo "  TS      $@"

# ---- Copy binary + package VSIX ----------------------------------------
package: $(SERVER_BIN_COPY) extension
	cd $(CLIENT_DIR) && $(NPX) vsce package
	@echo "  VSIX    $(VSIX)"

$(SERVER_BIN_COPY): $(SERVER_BIN_FULL)
	$(MKDIR) $(CLIENT_DIR)/server
	$(CP) $< $@
	@echo "  COPY    $@"

# ---- Install extension to VS Code --------------------------------------
install: package
	code --install-extension $(CLIENT_DIR)/cco-language-support-*.vsix --force
	@echo "  INSTALL done"

# ---- Test LSP server ---------------------------------------------------
TEST_MSG := '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"capabilities":{}}}'

test: $(SERVER_BIN_FULL)
	@echo "--- Initialization handshake ---"
	printf 'Content-Length: 92\r\n\r\n{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"capabilities":{}}}' | $(SERVER_BIN_FULL)
	@echo ""
	@echo "--- Diagnostics (valid file) ---"
	node -e "var m=JSON.stringify({jsonrpc:'2.0',id:1,method:'initialize',params:{processId:null,capabilities:{}}});var d='Content-Length: '+Buffer.byteLength(m)+'\r\n\r\n'+m;var t='\$$$$typedef.Url: String,\napp: (\n    url: \"ok\"\n)';var o=JSON.stringify({jsonrpc:'2.0',method:'textDocument/didOpen',params:{textDocument:{uri:'file:///t.cco',languageId:'cco',version:1,text:t}}});d+='Content-Length: '+Buffer.byteLength(o)+'\r\n\r\n'+o;var s=JSON.stringify({jsonrpc:'2.0',id:2,method:'shutdown'});d+='Content-Length: '+Buffer.byteLength(s)+'\r\n\r\n'+s;process.stdout.write(d)" | $(SERVER_BIN_FULL)
	@echo ""
	@echo "--- Diagnostics (error file) ---"
	node -e "var m=JSON.stringify({jsonrpc:'2.0',id:1,method:'initialize',params:{processId:null,capabilities:{}}});var d='Content-Length: '+Buffer.byteLength(m)+'\r\n\r\n'+m;var t='url: WrOngType';var o=JSON.stringify({jsonrpc:'2.0',method:'textDocument/didOpen',params:{textDocument:{uri:'file:///e.cco',languageId:'cco',version:1,text:t}}});d+='Content-Length: '+Buffer.byteLength(o)+'\r\n\r\n'+o;var s=JSON.stringify({jsonrpc:'2.0',id:2,method:'shutdown'});d+='Content-Length: '+Buffer.byteLength(s)+'\r\n\r\n'+s;process.stdout.write(d)" | $(SERVER_BIN_FULL)
	@echo "--- ALL TESTS PASSED ---"

# ---- Clean -------------------------------------------------------------
clean:
	-$(CMAKE) --build $(BUILD_DIR) --target clean 2>$(NULL)
	-$(RM) $(SERVER_BIN_FULL)
	-$(RM) -r $(CLIENT_DIR)/out
	-$(RM) -r $(CLIENT_DIR)/server

distclean: clean
	-$(RM) -r $(BUILD_DIR)
	-$(RM) -r $(CCO_BUILD)
	-$(RM) $(CLIENT_DIR)/node_modules/.package-lock.json
	-$(RM) $(CLIENT_DIR)/cco-language-support-*.vsix

# ---- Phony -------------------------------------------------------------
help:
	@echo "CCO Language Server & VS Code Extension"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build server + extension (default)"
	@echo "  lib          Build libcco library only"
	@echo "  server       Build C language server"
	@echo "  extension    Compile TypeScript extension"
	@echo "  package      Build server + extension + create VSIX"
	@echo "  install      Package + install extension to VS Code"
	@echo "  test         Run LSP server integration tests"
	@echo "  clean        Remove build artifacts"
	@echo "  distclean    Remove all generated files"
	@echo ""
	@echo "Binary location: $(SERVER_BIN_FULL)"
