/*
 ** Ctis code is based on the standalone.cpp example code
 ** 
 **Semester Project Rosina Weiss, FS 2025, RSL --> HIPER Hand/Foot/Gripper
*/
#include "ethercat_device_configurator/EthercatDeviceConfigurator.hpp"
#include "message_logger/message_logger.hpp"

#ifdef _MAXON_FOUND_
#include <maxon_epos_ethercat_sdk/Maxon.hpp>
#endif


#include <csignal>
#include <thread>
// #include <termios.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <sys/select.h>
#include <ncurses.h>

namespace example {

class ExampleEcatHardwareInterface {
 public:
  ExampleEcatHardwareInterface() {
    // we could also do the init stuff during construction, but usually you want to have the init somewhere in a hardware interface state
    // machine.
  }

  bool init(char* pathToConfigFile) {
    configurator_ = std::make_shared<EthercatDeviceConfigurator>();
    configurator_->initializeFromFile(pathToConfigFile);

    ecatMaster_ = configurator_->master();  // throws if more than one master.

    // get a list
#ifdef _MAXON_FOUND_
    maxons_ = configurator_->getSlavesOfType<maxon::Maxon>(EthercatDeviceConfigurator::EthercatSlaveType::Maxon);
#endif

    // starts up the bus and all slaves, blocks till the bus is in SAFE_OP state therefore cyclic PDO reading thread can be started, and
    // other operations can be performed in between. when the bus is directly put into OP state with startup(true) a watchdog on the slave
    // is started which checks if cyclic PDO is happening, if this communication is not started fast enough the drive goes into an error
    // state.
    if (ecatMaster_->startup(startupAbortFlag_)) {
      MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] Successfully started Ethercat Master on Network Interface: "
                       << ecatMaster_->getBusPtr()->getName());
    } else {
      MELO_ERROR_STREAM("[EthercatDeviceConfiguratorExample] Could not start the Ethercat Master.")
      return false;
    }

    // slaves are no in SAFE_OP state. SDO communication is available - special SDO config calls should be done in the slaves startup
    // memeber function which is called by the ecatMaster->startup() or here.

    workerThread_ = std::make_unique<std::thread>([this]() -> void {
      if (ecatMaster_->setRealtimePriority(48)) {  // do not set above 48, otherwise starve kernel processes on which soem depends.
        MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] Set increased thread priority to 48")
      } else {
        MELO_WARN_STREAM("[EthercatDeviceConfiguratorExample] Could not incrase thread priority - check user privileges.")
      }

      // we could also do it outside of the thread after we knew it started looping.
      if (ecatMaster_->activate()) {
        MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] Activated the Bus: " << ecatMaster_->getBusPtr()->getName())
      }
      // here the watchdog on the slave is activated. therefore don't block/sleep for 100ms..
      while (!abrtFlag_) {
        ecatMaster_->update(ecat_master::UpdateMode::StandaloneEnforceStep);
        // we could have interaction with some slaves here, e.g. getting and setting commands. this is than in sync with the ethercat loop.
        // but we have to be carefully to no block it too long. otherwise problems with certain slaves.
      }
      // make sure that bus is in SAFE_OP state, if preShutdown(true) should already do it, but makes sense to have this call here.
      ecatMaster_->deactivate();
    });

    return true;
  }

    void someUserStartInteraction() {
    // this would be user interaction e.g. publishing, subscribing, writing to shared memory whatever.
    // be aware that this is a concurrent interaction with stageCommand, getReading of the slaves. If you do this to fast you might starve
    // the ethercat cyclic loop which might cause problems with certain slaves, which require a strict timing. e.g. drives in cyclic
    // synchronous modes (csp, csv, cst, csc) with those drives you also have to check that interpolationIndex is set so that it reflects
    // the update time of the ethercat loop.

    #ifdef _MAXON_FOUND_
        for (auto& maxon : maxons_) {
          // Set maxons to operation enabled state, do not block the call!
          maxon->setDriveStateViaPdo(maxon::DriveState::OperationEnabled, true);  // todo check
        }
    #endif
  } 

  void interactiveControl() {
    #ifdef _MAXON_FOUND_
    static double velocity1 = 0.0;
    static double velocity2 = 0.0;
    const double VELOCITY = 30.0;
    static char activeKey = 0;
    static std::chrono::steady_clock::time_point lastKeyTime = std::chrono::steady_clock::now();
    const auto TIMEOUT = std::chrono::milliseconds(200);

    int key = getch();
    if (key != ERR) {
      activeKey = static_cast<char>(key);
      lastKeyTime = std::chrono::steady_clock::now();
    }

    // Timeout detection (simulate key release)
    auto now = std::chrono::steady_clock::now();
    if (now - lastKeyTime > TIMEOUT) {
      velocity1 = 0.0;
      velocity2 = 0.0;
      activeKey = 0;
    }
   

    //Key Logic: w/e for motor 1 and s/d for motor 2;
    switch (key){
      case 'w': velocity1 = +VELOCITY; break;
      case 'e': velocity1 = -VELOCITY; break;
      case 's': velocity2 = +VELOCITY; break;
      case 'd': velocity2 = -VELOCITY; break;
      case ' ': velocity1 = velocity2 = 0.0; break;
      default: break;      
    }

    mvprintw(1,0, "Active Key: %c       ", activeKey ? activeKey : ' ');
    mvprintw(2,0, "Velocity1: %0.2f      ", velocity1);
    mvprintw(3,0, "Velocity2: %0.2f      ", velocity2);
    refresh();


    
      for(size_t i=0; i < maxons_.size(); ++i){
          auto& motor = maxons_[i];
          if(!motor->lastPdoStateChangeSuccessful() || motor->getReading().getDriveState() != maxon::DriveState::OperationEnabled) {
            MELO_WARN_STREAM("Motor " << motor->getName() << "not operational.");
            continue;
          }
          maxon::Command command;
          
          if (i == 0){
            if (velocity1 == 0){
              command.setModeOfOperation(maxon::ModeOfOperationEnum::CyclicSynchronousPositionMode);
              command.setTargetPosition(motor->getReading().getActualPosition());
            } else {
              command.setModeOfOperation(maxon::ModeOfOperationEnum::CyclicSynchronousVelocityMode);
              command.setTargetVelocity(velocity1);
            }
          } else if (i==1){
            if (velocity2 == 0){
              command.setModeOfOperation(maxon::ModeOfOperationEnum::CyclicSynchronousPositionMode);
              command.setTargetPosition(motor->getReading().getActualPosition());
            } else {
              command.setModeOfOperation(maxon::ModeOfOperationEnum::CyclicSynchronousVelocityMode);
              command.setTargetVelocity(velocity2);
            }
          }
          
          motor->stageCommand(command);
        }
    #endif
  }



  void cyclicUserInteraction() {
    userCyclicThread_ = std::make_unique<std::thread>([this]() {
      // TerminalSettings terminal;
      // terminal.enableNonBlockingMode();
      initscr();             // Start ncurses
      cbreak();              // Disable line buffering
      noecho();              // Do not print typed characters
      nodelay(stdscr, TRUE); // Make getch() non-blocking
      keypad(stdscr, TRUE);  // Enable arrow keys, etc.

      while (userInteraction_) {
        // this can run fully async, as here! but be aware that we're doing concurrent blocking calls into the time sensitive cyclic PDO
        // loop. there are multiple ways to avoid/improve this e.g. syncing this interaction with the cyclic PDO loop with conditional
        // variables e.g. queue the readings into a (lock-free) fancy producer consumer queue e.g. copy out the readings in a callback. (the
        // call to it is than again synced into cyclic PDO loop)
        interactiveControl();
        // if (maxons_.size() >= 2) {
        // auto motor1 = maxons_.at(0);
        // auto motor2 = maxons_.at(1);
        
        // static int counter = 0;
        // static int direction = 1;
        // //static int simulatedPosition = 0;

        // if (motor1->lastPdoStateChangeSuccessful() && motor1->getReading().getDriveState() == maxon::DriveState::OperationEnabled) {
        //   maxon::Command command1;
        //   command1.setModeOfOperation(maxon::ModeOfOperationEnum::CyclicSynchronousPositionMode);  // todo torque mode with positon command?
        //   auto reading1 = motor1->getReading();
        //   command1.setTargetPosition(reading1.getActualPosition() + 10);
        //   motor1->stageCommand(command1);
        // }
        // if (motor2->lastPdoStateChangeSuccessful() && motor2->getReading().getDriveState() == maxon::DriveState::OperationEnabled) {
        //   maxon::Command command2;
        //   command2.setModeOfOperation(maxon::ModeOfOperationEnum::CyclicSynchronousPositionMode);  // todo torque mode with positon command?
        //   auto reading2 = motor2->getReading();
        //   if (counter++ % 100 == 0) direction *= -1;
        //   command2.setTargetPosition(reading2.getActualPosition() + direction * 10);
        //   motor2->stageCommand(command2);
        // }
        // } else {
        //   MELO_WARN_STREAM("[EthercatDeviceConfiguratorExample] MCPA not in operationEnabled state: "
        //                   << maxons_.at(0)->getName() << "': " << maxons_.at(0)->getReading().getDriveState());
        //   MELO_WARN_STREAM("[EthercatDeviceConfiguratorExample] MCPF not in operationEnabled state: "
        //                   << maxons_.at(1)->getName() << "': " << maxons_.at(1)->getReading().getDriveState());
        // }


        std::this_thread::sleep_for(std::chrono::milliseconds(5)); //original 5ms
      
      }
      // terminal.restore();
      endwin();
    });
  }

  void shutdown() {
    // stop the user code.
    userInteraction_ = false;
    if (userCyclicThread_) {
      if (userCyclicThread_->joinable()) {
        userCyclicThread_->join();
      }
    }
    MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] Shutdown user cyclic thread")

    // call preShutdown before terminating the cyclic PDO communication!!
    if (ecatMaster_) {
      ecatMaster_->preShutdown(true);
    }
    // we can do more shutdown stuff here. since bus is in SAFE OP there is no strange PDO timeout which we might trigger.
    MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] PreShutdown ethercat master and all slaves.")

    abrtFlag_ = true;
    if (workerThread_) {
      if (workerThread_->joinable()) {
        workerThread_->join();
      }
    }
    MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] Joined the ethercat master.")

    if (ecatMaster_) {
      ecatMaster_->shutdown();
    }
    MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] Fully shutdown.")
  }

  void abortStartup() { startupAbortFlag_ = true; }

  ~ExampleEcatHardwareInterface() {
    // if no signal handler is used - we can do this in the destructor.
    MELO_INFO_STREAM("[EthercatDeviceConfiguratorExample] Destructor.")
    shutdown();
  }

 private:
  EthercatDeviceConfigurator::SharedPtr configurator_;
  ecat_master::EthercatMaster::SharedPtr ecatMaster_;

// cache the devices on the bus, since topology of the robot does not change dynamically.
// in the more general case you would do this per bus. e.g. you have a robot with two ethercat buses.
// in general use one bus per robot - that's the main motivation of having a bus.
#ifdef _MAXON_FOUND_
  bool maxonEnabledAfterStartup_ = false;
  std::vector<std::shared_ptr<maxon::Maxon>> maxons_{};
#endif


  std::unique_ptr<std::thread> workerThread_;
  std::unique_ptr<std::thread> userCyclicThread_;
  std::atomic<bool> startupAbortFlag_{false};
  std::atomic<bool> abrtFlag_{false};
  std::atomic<bool> userInteraction_{true};
};

class SimpleSignalHandler {
 public:
  static void sigIntCall([[maybe_unused]] int signal) {
    MELO_INFO_STREAM(" ** Signal Handling **")
    if (!sigIntCallbacks_.empty()) {
      for (auto& sigIntCallback : sigIntCallbacks_) {
        sigIntCallback(signal);
      }
    }
  }

  static void registerSignalHandler() { std::signal(SIGINT, SimpleSignalHandler::sigIntCall); }

  static void addSigIntCallback(std::function<void(int)> callback) { sigIntCallbacks_.push_back(callback); }

  static std::vector<std::function<void(int)>> sigIntCallbacks_;
};

std::vector<std::function<void(int)>> SimpleSignalHandler::sigIntCallbacks_ = {};
};  // namespace example

/*
** Program entry.
** Pass the path to the setup.yaml file as first command line argument.
*/
int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "pass path to 'setup.yaml' as command line argument" << std::endl;
    return EXIT_FAILURE;
  }

  example::ExampleEcatHardwareInterface exampleEcatHardwareInterface{};
  std::atomic<bool> shutdownFlag{false};

  example::SimpleSignalHandler::addSigIntCallback([&shutdownFlag, &exampleEcatHardwareInterface]([[maybe_unused]] int signal) {
    exampleEcatHardwareInterface.abortStartup();
    shutdownFlag = true;
  });

  example::SimpleSignalHandler::registerSignalHandler();

  if (exampleEcatHardwareInterface.init(argv[1])) {
    MELO_INFO_STREAM("[EthercatExmample] Startup completed.")
    exampleEcatHardwareInterface.someUserStartInteraction();
    exampleEcatHardwareInterface.cyclicUserInteraction();
  }
  // nothing further to do in this thread, so we wait till we receive a shutdown signal s.t later the exmpaleEcatHArdwareInterface can be
  // nicely shutdown the shutdown method should NOT be in the signal handler because the signal handler has to be lock-free, otherwise UB.
  // https://en.cppreference.com/w/cpp/utility/program/signal
  MELO_INFO_STREAM("[Main Thread] started waiting...")
  while (!shutdownFlag) {  // could be replaced with smarter stuff like a conditional variable
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  MELO_INFO_STREAM("[Main Thread] stopped waiting... shut down.. ")
  exampleEcatHardwareInterface.shutdown();
  return 0;
}
