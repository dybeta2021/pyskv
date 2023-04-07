//
// Created by 稻草人 on 2022/8/7.
//
// https://blog.csdn.net/qq_35608277/article/details/80071408
#include "interface.h"
#include "logger.h"


PYBIND11_MODULE(pyskv, m) {
    py::class_<PySKV>(m, "PySKV")
            .def(py::init<const std::string &,
                          const size_t &,
                          const size_t &,
                          const bool &,
                          const bool &,
                          const bool &,
                          const std::string &,
                          const std::string &,
                          const bool &,
                          const bool &,
                          const std::string &>())

            .def("Set", &PySKV::Set, "Set", py::arg("key"), py::arg("value"), py::arg("value_len"), py::arg("process_lock"), py::arg("thread_lock"))
            .def("Get", &PySKV::Get, "Get", py::arg("key"))
            .def("Del", &PySKV::Del, "Del", py::arg("key"), py::arg("process_lock"), py::arg("thread_lock"))
            .def("GetCurrentAllKeys", &PySKV::GetCurrentAllKeys, "GetCurrentAllKeys")
            .def("ShowHeader", &PySKV::ShowHeader, "ShowHeader")
            .def("ShowCurrentKey", &PySKV::ShowCurrentKey, "ShowCurrentKey")
            .def("ShowAllKey", &PySKV::ShowAllKey, "ShowAllKey");
}
