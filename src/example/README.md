
## 一、Clang LibTooling 工具：module_init 監聽與參數擷取

* 在 `wdtesting/src/instrument-tool/module_init_macro.cpp` 新增一個 **Clang LibTooling 範例工具**，負責在前處理階段攔截 `module_init(...)` 的使用。
* 透過 `PPCallbacks` 註冊前處理監聽器 `ModuleInitListener`，實作 `MacroExpands`：

  * 當偵測到名稱為 `module_init` 的宏展開時，會取出整段呼叫的原始文字（例如：`module_init(my_pci_init)`）。
  * 解析括號內部的參數，將結果輸出為：

    ```text
    [module_init] invocation at <loc> -> arg: <arg>
    ```

    其中 `<loc>` 為原始碼位置，`<arg>` 為實際的 init 函式名稱（如 `my_pci_init`）。
* 定義 `ModuleInitAction`，在 `BeginSourceFileAction` 內將 `ModuleInitListener` 掛到 Preprocessor，不額外做 AST 分析，專注於 macro 層級的事件監聽。
* 已在 `wdtesting/src/instrument-tool/Makefile` 中新增 `module_init_macro` 目標，用於編譯這個工具。
* 目前編譯狀況：

  * 嘗試 `make module_init_macro` 時失敗，原因是系統上缺少 `llvm-config` 以及 `clang/Frontend/CompilerInstance.h` 等開發用標頭與 library。
  * 後續需要在環境中安裝 **LLVM/Clang 開發套件（含 libclang / llvm-dev / llvm-config）** 才能成功編譯這個工具。

---

## 二、在 pci_debugfs.c 中加入 module_init 階段的 DebugFS 介面

* 檔案位置：`wdtesting/src/example/pci_debugfs.c`。
* 在 **module 初始化階段（`my_pci_init`）** 新增一組獨立的 DebugFS 介面，用來觀察與觸發「模組層級的測試狀態」：

  * 建立一個模組層級的 DebugFS 目錄：`module_init_debug`。
  * 在該目錄底下提供兩個檔案：

    1. `start_test`（write-only）

       * 寫入任意內容時：

         * 將字串記錄到 `module_test_info`（模組內的狀態變數）。
         * 將狀態標記為「test running」。
         * 在 `dmesg` 中輸出 log 以便觀察。
    2. `get_test_info`（read-only）

       * 讀取時回傳目前的 `module_test_info` 內容。
       * 若尚未透過 `start_test` 啟動，則回傳 `"idle"`。
* 實作上：

  * 使用 `debugfs_create_dir()` 建立 `module_init_debug` 目錄，並以 `debugfs_create_file()` 建立上述兩個檔案。
  * 針對 `start_test` / `get_test_info` 各自定義 `file_operations` 結構：

    * `start_test` 使用 `write` callback，搭配 `simple_write_to_buffer` 處理 user space 寫入。
    * `get_test_info` 使用 `read` callback，搭配 `simple_read_from_buffer` 回傳目前訊息。
  * 引入 `<linux/uaccess.h>` 以支援 user space buffer 的存取。
  * 在 `my_pci_exit` 中呼叫 `debugfs_remove_recursive()` 清除 `module_init_debug` 目錄，避免殘留。
* 使用方式（載入模組後）：

  ```bash
  echo "run X" > /sys/kernel/debug/module_init_debug/start_test
  cat /sys/kernel/debug/module_init_debug/get_test_info
  ```

  如需調整目錄名稱或檔名，只要修改 `module_debugfs_dir` 的建立位置與 `debugfs_create_file` 的名稱即可。

---

## 三、抽出 helper 並從 module_init 呼叫新建函式

* 為了讓 **module_init 階段的行為更清楚，也符合「在 module_init 中呼叫新函式」的需求**，在 `pci_debugfs.c` 中新增了一個 helper：

  * 新增 `create_module_debugfs()`：

    * 封裝 `module_init_debug` 目錄與 `start_test`／`get_test_info` 兩個 DebugFS 檔案的建立流程。
    * 負責初始化與錯誤處理，讓 `my_pci_init` 保持簡潔。
  * 修改 `my_pci_init()`：

    * 於原本的初始化流程中，改為直接呼叫 `create_module_debugfs()`。
    * 原有模組載入與 driver 註冊行為保持不變，只是多了一個明確的「建立模組 DebugFS 介面」步驟。
* 這樣的重構同時達成兩個目標：

  1. **語意清楚**：`my_pci_init()` 看得出來有一個單獨負責 DebugFS 設定的函式。
  2. **對 Clang 插樁友善**：未來若要用 Clang 在 `my_pci_init` 自動插入其他呼叫（例如測試前置程式），可以直接在這個函式前後增加程式碼，不會和 DebugFS 的細節交纏。


