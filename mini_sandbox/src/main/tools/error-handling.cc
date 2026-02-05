/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */


#include "error-handling.h"
#include "src/main/tools/logging.h"
#include "src/main/tools/linux-sandbox-options.h"
#include "src/main/tools/process-tools.h"


#if defined(LIBMINISANDBOX)
Mode mode = Mode::Library;
#else
Mode mode = Mode::CLI;
#endif

MiniSbxError sbx_err;


static void GenErrorMessage(const std::string& err_msg, const char* file, 
                                          int line, const char* func, std::string& out) {
  out = "[" + std::string(func) + ":" + std::to_string(line) + "]: "  + err_msg;
}


static void MiniSbxSetError(const std::string& err_msg, ErrorCode code, int& ret) {
  bool recoverable = (static_cast<int>(code) < RECOVERABLE_ERROR_CODES);
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
  if (recoverable) 
    ret = RECOVERABLE_FAIL;
  else
    ret = UNRECOVERABLE_FAIL;
}


int MiniSbxReportGenericError_impl(const std::string& err_msg, const char* file, int line, const char* func) {
  return MiniSbxReportErrorAndMessage_impl(err_msg, ErrorCode::GeneralOSError, file, line, func);
}


int MiniSbxReportError_impl(ErrorCode code, const char* file, int line, const char* func) {
  return MiniSbxReportErrorAndMessage_impl("", code, file, line, func);
}


int MiniSbxReportErrorAndMessage_impl(std::string err_msg, ErrorCode code, const char* file, int line, const char* func) {
  std::string msg; 
  std::string code_msg;
  int ret = 0;
  
  code_msg = GetErrorMessage(code);
  if (!err_msg.empty())
    code_msg += ": " + err_msg;

  GenErrorMessage(code_msg, file, line, func, msg);
  MiniSbxSetError(msg, code, ret);
  return ret;
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
