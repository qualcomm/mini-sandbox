#include "error_handling.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/process-tools.h"


#if defined(LIBMINISANDBOX)
Mode mode = Mode::Library;
#else
Mode mode = Mode::CLI;
#endif

#define UNRECOVERABLE_FAIL -1
#define RECOVERABLE_FAIL -2

MiniSbxError sbx_err;

static void MiniSbxSetError(const std::string& err_msg, ErrorCode code, bool recoverable) {
  if (mode == Mode::Library || recoverable) {
    memset(sbx_err.msg, 0, MAX_ERR_LEN - 1);
    strncpy(sbx_err.msg, err_msg.c_str(), MAX_ERR_LEN - 1);
    sbx_err.msg[MAX_ERR_LEN - 1] = '\0';
    sbx_err.code = code;
  }
  else {
     Cleanup();
     fprintf(stderr, "%s\n", err_msg.c_str());
     exit(EXIT_FAILURE);
  }
}

int MiniSbxReportGenericRecoverableError(const std::string& err_msg) {
  MiniSbxSetError(err_msg, ErrorCode::GeneralOSError, true);
  return RECOVERABLE_FAIL;
}


int MiniSbxReportGenericError(const std::string& err_msg) {
  MiniSbxSetError(err_msg, ErrorCode::GeneralOSError, false );
  return UNRECOVERABLE_FAIL;
}


int MiniSbxReport(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    size_t size = std::vsnprintf(nullptr, 0, fmt, args) + 1;
    va_end(args);

    std::vector<char> buffer(size);
    va_start(args, fmt);
    std::vsnprintf(buffer.data(), size, fmt, args);
    va_end(args);

    return MiniSbxReportGenericError(std::string(buffer.data()));
}



int MiniSbxReportError(std::string function, ErrorCode code) {
  std::string err_msg = function + GetErrorMessage(code);
  // We handle all errors at the same way in the library but we
  // could potentially force some of them to crash the application
  // depending on their importance
  MiniSbxSetError(err_msg, code, false);
  return UNRECOVERABLE_FAIL; 
}


MiniSbxError MiniSbxGetLastError() {
  return sbx_err;
}

const char* MiniSbxGetErrorMsg() {
  return sbx_err.msg;
}

int MiniSbxGetErrorCode() {
  return static_cast<int>(sbx_err.code);
}
