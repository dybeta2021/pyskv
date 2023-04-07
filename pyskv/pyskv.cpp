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
            .def("Set", &PySKV::Set)
            .def("Get", &PySKV::Get)
            .def("Del", &PySKV::Del)
            .def("GetCurrentAllKeys", &PySKV::GetCurrentAllKeys)
            .def("ShowHeader", &PySKV::ShowHeader)
            .def("ShowCurrentKey", &PySKV::ShowCurrentKey)
            .def("ShowAllKey", &PySKV::ShowAllKey);
}
