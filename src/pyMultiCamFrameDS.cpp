
#include <string>
#include <dlfcn.h>
#include <cstdlib>      // getenv / _dupenv_s

#include "utils/common_wrapper.hpp"
#include "mutiCamFrameDS.h"

#include <libgen.h>   // 用于 dirname()
#include <limits.h>   // 用于 PATH_MAX


static void* g_xml_parser_handle = nullptr; // 动态库句柄

PYBIND11_MODULE(multicamframeDS, m)
{
    m.doc() = "pybind11 get frame from DS camera by ip";

    char path[PATH_MAX] = {0}; // 使用 PATH_MAX 确保足够长度

    // 优先检查环境变量中的路径
    char* env_value = nullptr;
#if defined(_WIN32)
    size_t env_size = 0;
    if (_dupenv_s(&env_value, &env_size, "CAM_DS_LIB_PATH") != 0) {
        env_value = nullptr;
    }
#else
    env_value = getenv("CAM_DS_LIB_PATH");
#endif

    if (env_value != nullptr) {
        snprintf(path, sizeof(path), "%s/libXmlParser_gcc42_v3_1.so", env_value);
#if defined(_WIN32)
        free(env_value);
#endif
    } else {
        // 获取当前模块的路径
        pybind11::module_ sys_modules = pybind11::module_::import("sys");
        pybind11::object module = sys_modules.attr("modules")[m.attr("__name__")];
        std::string module_path = module.attr("__file__").cast<std::string>();

        // 提取目录部分
        char module_dir[PATH_MAX];
        strncpy(module_dir, module_path.c_str(), sizeof(module_dir));
        dirname(module_dir); // 获取所在目录

        // 构造库路径
        snprintf(path, sizeof(path), "%s/libXmlParser_gcc42_v3_1.so", module_dir);
    }

    printf("Loading libXmlParser from: %s\n", path);

    g_xml_parser_handle = dlopen(path, RTLD_NOW);
    if (g_xml_parser_handle == nullptr) {
        printf("dlopen failed: %s\n", dlerror());
        return;
    } else {
        printf("dlopen success\n");
    }

    pybind11::class_<Struct_FRAME>(m, "Frame")
        .def(pybind11::init<>())
        .def_readwrite("result", &Struct_FRAME::result)
        .def_property("frame",
            [](const Struct_FRAME& self) {
                return utils::MatToArray(self.frame);
            },
            [](Struct_FRAME& self, pybind11::array_t<uint8_t>& array) {
                self.frame = utils::ArrayToMat(array);
            })
        .def_readwrite("relativeTimestamp", &Struct_FRAME::relativeTimestamp);

    pybind11::class_<MultiCamFrameDS>(m, "MultiCamFrameDS")
        .def(pybind11::init<int>())
        .def("scan_cameras", &MultiCamFrameDS::ScanCamers,
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("create_camera", &MultiCamFrameDS::CreateCamera,
            pybind11::call_guard<pybind11::gil_scoped_release>(),
            pybind11::arg("ip"),
            pybind11::arg("file_path"))
        .def("init", &MultiCamFrameDS::Init,
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("set_mode", &MultiCamFrameDS::SetMode)
        .def("set_rotate", &MultiCamFrameDS::SetRotate,
            pybind11::arg("rotate_code"))
        .def("set_resize", &MultiCamFrameDS::SetResize,
            pybind11::arg("width"), pybind11::arg("height"))
        .def("set_max_reconnect", &MultiCamFrameDS::SetMaxReconnect,
            pybind11::arg("count"))
        .def("set_max_backup_count", &MultiCamFrameDS::SetMaxBackupCount,
            pybind11::arg("count"))
        .def("read", &MultiCamFrameDS::Read,
            pybind11::call_guard<pybind11::gil_scoped_release>())
        .def("read_t", &MultiCamFrameDS::Read_t,
            pybind11::call_guard<pybind11::gil_scoped_release>(),
            pybind11::arg("time_wait") = 50)
        .def("read_batch_t", &MultiCamFrameDS::ReadBatch_t,
            pybind11::call_guard<pybind11::gil_scoped_release>(),
            pybind11::arg("time_wait") = 50)
        .def("release", &MultiCamFrameDS::Release,
            pybind11::call_guard<pybind11::gil_scoped_release>());
}
