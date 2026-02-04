/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */

#ifndef _ERROR_HANDLING_H
#define _ERROR_HANDLING_H

#include <iostream>
#include <cstdio>
#include <cerrno>
#include <string>
#include <cstring>
#include <cstdarg>
#include <vector>
#include <cstring>

#define MAX_ERR_LEN 255

#define UNRECOVERABLE_FAIL -1
#define RECOVERABLE_FAIL -2
#define RECOVERABLE_ERROR_CODES -200



enum class Mode {
    CLI,
    Library
};

enum class ErrorCode : int {
  None = 0,
  InvalidFunctioningMode = -1,
  SandboxRootNotUnique = -2,
  InvalidFolder = -3,
  OverlayOptionNotSet = -4,
  PathDoesNotExist = -5,
  NotAnAbsolutePath = -6,
  LogFileNotUnique = -7,
  IllegalConfiguration = -8,
  FileReadAndWrite = -9,
  IllegalNetworkConfiguration = -10,
  TmpNotRemounted = -11,
  GeneralOSError = -100,
  // Error codes from -201 are recoverables
  NestedSandbox = -201,
  Unknown = -1000
};

inline std::string GetErrorMessage(ErrorCode code) {
  switch (code) {
    case ErrorCode::None:
      return ": No error";
    case ErrorCode::InvalidFunctioningMode:
      return ": Only one between default, overlayfs and hermetic must be used";
    case ErrorCode::SandboxRootNotUnique:
      return " : Only one sandbox root directory is allowed";
    case ErrorCode::InvalidFolder:
      return " : Specify a valid directory";
    case ErrorCode::OverlayOptionNotSet:
      return " : Overlay option not enabled";
    case ErrorCode::PathDoesNotExist:
      return " : Path does not exist";
    case ErrorCode::NotAnAbsolutePath:
      return " : Must use absolute paths";
    case ErrorCode::LogFileNotUnique:
      return " : Cannot write debug output to more than one file";
    case ErrorCode::IllegalConfiguration:
      return " : Illegal configuration. Overlay folder can't be inside a writable directory";
    case ErrorCode::FileReadAndWrite:
      return " : Illegal configuration. File mounted as read and write at the same time";
    case ErrorCode::NestedSandbox:
      return " : Cannot nest multiple sandbox. The process is already running inside mini-sandbox.";
    case ErrorCode::GeneralOSError:
      return " : OS Error";
    case ErrorCode::IllegalNetworkConfiguration:
      return " : Cannot allow all domains after specifying one network rule";
    case ErrorCode::TmpNotRemounted:
      return " : /tmp cannot be remounted when running in default mode";
    case ErrorCode::Unknown:
    default:
      return ": Unknown error occurred";
  }
}

typedef struct {
  char msg[MAX_ERR_LEN];
  ErrorCode code;
} MiniSbxError;

#define MiniSbxReportGenericError(msg) \
    MiniSbxReportGenericError_impl((msg), __FILE__, __LINE__, __func__)

int MiniSbxReportGenericError_impl(const std::string& err_msg, const char* file, int line, const char* func);

#define MiniSbxReportError(code) \
    MiniSbxReportError_impl((code), __FILE__, __LINE__, __func__)

int MiniSbxReportError_impl(ErrorCode code, const char* file, int line, const char* func);

#define MiniSbxReportErrorAndMessage(msg, code) \
    MiniSbxReportErrorAndMessage_impl((msg), (code), __FILE__, __LINE__, __func__)

int MiniSbxReportErrorAndMessage_impl(std::string err_msg, ErrorCode code, const char* file, int line, const char* func);

MiniSbxError MiniSbxGetError();
const char* MiniSbxGetErrorMsg();
int MiniSbxGetErrorCode();

#endif

