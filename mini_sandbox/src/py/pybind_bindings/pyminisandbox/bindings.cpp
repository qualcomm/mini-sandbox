/*
 * Copyright (c) 2025 Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: MIT
 */
#include <pybind11/pybind11.h>
#include "linux-sandbox-api.h"


namespace py = pybind11;
#ifdef MINITAP
PYBIND11_MODULE(pyminitapbox, m) {
#else
PYBIND11_MODULE(pyminisandbox, m) {
#endif
    m.doc() = "Python bindings for libmini-sandbox";
    m.def("mini_sandbox_start", &mini_sandbox_start, "Start the mini sandbox");
    m.def("mini_sandbox_setup_default", &mini_sandbox_setup_default, "Set default sandbox configuration");
#ifndef MINITAP
    m.def("mini_sandbox_share_network", &mini_sandbox_share_network, "Share network namespace with host machine");
#endif
    m.def("mini_sandbox_mount_bind", &mini_sandbox_mount_bind, py::arg("path"), "Bind mount a path");
    m.def("mini_sandbox_mount_write", &mini_sandbox_mount_write, py::arg("path"), "Make a path writable");
    m.def("mini_sandbox_mount_empty_output_file", &mini_sandbox_mount_empty_output_file, py::arg("path"), "Make a path writable");
    m.def("mini_sandbox_enable_log", &mini_sandbox_enable_log, py::arg("path"), "Set a path where to store the log");

    m.def("mini_sandbox_get_last_error_msg", []() -> std::string { return std::string(mini_sandbox_get_last_error_msg()); });

#ifdef MINITAP
    m.def("mini_sandbox_allow_ipv4", &mini_sandbox_allow_ipv4, py::arg("ipv4"), "Add the IP address in allow-list. Should be called before mini_sandbox_start");
    m.def("mini_sandbox_allow_connections", &mini_sandbox_allow_connections, py::arg("path"), "Set firewall rules accordingly to a list of IPs/Domains to allow");
    m.def("mini_sandbox_allow_domain", &mini_sandbox_allow_domain, py::arg("domain"), "Add the domain  allow-list. Should be called before mini_sandbox_start");
    m.def("mini_sandbox_allow_all_domains", &mini_sandbox_allow_all_domains, "Allow all domains. Should be called before mini_sandbox_start");
    m.def("mini_sandbox_allow_ipv4_subnet", &mini_sandbox_allow_ipv4_subnet, py::arg("subnet"), "Allow all domains. Should be called before mini_sandbox_start");

#endif
}
