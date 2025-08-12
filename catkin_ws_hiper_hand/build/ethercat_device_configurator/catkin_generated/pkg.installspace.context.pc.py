# generated from catkin/cmake/template/pkg.context.pc.in
CATKIN_PACKAGE_PREFIX = ""
PROJECT_PKG_CONFIG_INCLUDE_DIRS = "${prefix}/include".split(';') if "${prefix}/include" != "" else []
PROJECT_CATKIN_DEPENDS = "anydrive_rsl;rokubimini_rsl_ethercat_slave;maxon_epos_ethercat_sdk;param_io;ethercat_sdk_master".replace(';', ' ')
PKG_CONFIG_LIBRARIES_WITH_PREFIX = "-lethercat_device_configurator".split(';') if "-lethercat_device_configurator" != "" else []
PROJECT_NAME = "ethercat_device_configurator"
PROJECT_SPACE_DIR = "/home/zrene/catkin_ws_hiper_hand/install"
PROJECT_VERSION = "0.6.0"
