#include "../Globals/Plugins.h"

#include "../CustomBuild/ESPEasyLimits.h"

#include "../../_Plugin_Helper.h"

#include "../DataStructs/ESPEasy_EventStruct.h"
#include "../DataStructs/TimingStats.h"

#include "../DataTypes/ESPEasy_plugin_functions.h"

#include "../ESPEasyCore/ESPEasy_Log.h"
#include "../ESPEasyCore/Serial.h"

#include "../Globals/Cache.h"
#include "../Globals/Device.h"
#include "../Globals/ESPEasy_Scheduler.h"
#include "../Globals/ExtraTaskSettings.h"
#include "../Globals/EventQueue.h"
#include "../Globals/GlobalMapPortStatus.h"
#include "../Globals/Settings.h"
#include "../Globals/Statistics.h"

#if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
#include "../Helpers/_Plugin_Helper_serial.h"
#endif

#include "../Helpers/ESPEasyRTC.h"
#include "../Helpers/ESPEasy_Storage.h"
#include "../Helpers/Hardware.h"
#include "../Helpers/Misc.h"
#include "../Helpers/_Plugin_init.h"
#include "../Helpers/PortStatus.h"
#include "../Helpers/StringConverter.h"
#include "../Helpers/StringParser.h"

#include <vector>




bool validDeviceIndex(deviceIndex_t index) {
  return validDeviceIndex_init(index);
}
/*
bool validTaskIndex(taskIndex_t index) {
  return index < TASKS_MAX;
}

bool validPluginID(pluginID_t pluginID) {
  return (pluginID != INVALID_PLUGIN_ID);
}
*/
bool validPluginID_fullcheck(pluginID_t pluginID) {
  return getDeviceIndex_from_PluginID(pluginID) != INVALID_DEVICE_INDEX;
}
/*
bool validUserVarIndex(userVarIndex_t index) {
  return index < USERVAR_MAX_INDEX;
}

bool validTaskVarIndex(taskVarIndex_t index) {
  return index < VARS_PER_TASK;
}
*/

bool supportedPluginID(pluginID_t pluginID) {
  return validDeviceIndex(getDeviceIndex(pluginID));
}

deviceIndex_t getDeviceIndex_from_TaskIndex(taskIndex_t taskIndex) {
  if (validTaskIndex(taskIndex)) {
    return getDeviceIndex(Settings.getPluginID_for_task(taskIndex));
  }
  return INVALID_DEVICE_INDEX;
}

/*********************************************************************************************
 * get the taskPluginID with required checks, INVALID_PLUGIN_ID when invalid
 ********************************************************************************************/
pluginID_t getPluginID_from_TaskIndex(taskIndex_t taskIndex) {
  if (validTaskIndex(taskIndex)) {
    const pluginID_t pluginID = Settings.getPluginID_for_task(taskIndex);
    if (supportedPluginID(pluginID))
      return pluginID;
  }
  return INVALID_PLUGIN_ID;
}

#if FEATURE_PLUGIN_PRIORITY
bool isPluginI2CPowerManager_from_TaskIndex(taskIndex_t taskIndex) {
  if (validTaskIndex(taskIndex)) {
    deviceIndex_t deviceIndex = getDeviceIndex_from_TaskIndex(taskIndex);
    if (validDeviceIndex(deviceIndex)) {
      return (Device[deviceIndex].Type == DEVICE_TYPE_I2C) && 
             Device[deviceIndex].PowerManager && 
             Settings.isPowerManagerTask(taskIndex);
    }
  }
  return false;
}
#endif // if FEATURE_PLUGIN_PRIORITY

deviceIndex_t getDeviceIndex(pluginID_t pluginID)
{
  return getDeviceIndex_from_PluginID(pluginID);
}

/********************************************************************************************\
   Find name of plugin given the plugin device index..
 \*********************************************************************************************/
String getPluginNameFromDeviceIndex(deviceIndex_t deviceIndex) {
  #ifdef USE_SECOND_HEAP
  HeapSelectDram ephemeral;
  #endif

  String deviceName;

  if (validDeviceIndex(deviceIndex)) {
    PluginCall(deviceIndex, PLUGIN_GET_DEVICENAME, nullptr, deviceName);
  }
  return deviceName;
}

String getPluginNameFromPluginID(pluginID_t pluginID) {
  deviceIndex_t deviceIndex = getDeviceIndex(pluginID);

  if (!validDeviceIndex(deviceIndex)) {
    String name = F("Plugin ");
    name += String(pluginID.value);
    name += F(" not included in build");
    return name;
  }
  return getPluginNameFromDeviceIndex(deviceIndex);
}

#if FEATURE_I2C_DEVICE_SCAN
bool checkPluginI2CAddressFromDeviceIndex(deviceIndex_t deviceIndex, uint8_t i2cAddress) {
  bool hasI2CAddress = false;

  if (validDeviceIndex(deviceIndex)) {
    String dummy;
    struct EventStruct TempEvent;
    TempEvent.Par1 = i2cAddress;
    hasI2CAddress = PluginCall(deviceIndex, PLUGIN_I2C_HAS_ADDRESS, &TempEvent, dummy);
  }
  return hasI2CAddress;
}
#endif // if FEATURE_I2C_DEVICE_SCAN

#if FEATURE_I2C_GET_ADDRESS
uint8_t getTaskI2CAddress(taskIndex_t taskIndex) {
  uint8_t getI2CAddress = 0;
  const deviceIndex_t deviceIndex = getDeviceIndex_from_TaskIndex(taskIndex);

  if (validTaskIndex(taskIndex) && validDeviceIndex(deviceIndex)) {
    String dummy;
    struct EventStruct TempEvent;
    TempEvent.setTaskIndex(taskIndex);
    TempEvent.Par1 = 0;
    if (PluginCall(deviceIndex, PLUGIN_I2C_GET_ADDRESS, &TempEvent, dummy)) {
      getI2CAddress = TempEvent.Par1;
    }
  }
  return getI2CAddress;
}
#endif // if FEATURE_I2C_GET_ADDRESS


// ********************************************************************************
// Functions to assist changing I2C multiplexer port or clock speed 
// when addressing a task
// ********************************************************************************

bool prepare_I2C_by_taskIndex(taskIndex_t taskIndex, deviceIndex_t DeviceIndex) {
  if (!validTaskIndex(taskIndex) || !validDeviceIndex(DeviceIndex)) {
    return false;
  }
  if (Device[DeviceIndex].Type != DEVICE_TYPE_I2C) {
    return true; // No I2C task, so consider all-OK
  }
  if (I2C_state != I2C_bus_state::OK) {
    return false; // Bus state is not OK, so do not consider task runnable
  }
  #if FEATURE_I2CMULTIPLEXER
  I2CMultiplexerSelectByTaskIndex(taskIndex);
  // Output is selected after this write, so now we must make sure the
  // frequency is set before anything else is sent.
  #endif // if FEATURE_I2CMULTIPLEXER

  if (bitRead(Settings.I2C_Flags[taskIndex], I2C_FLAGS_SLOW_SPEED)) {
    I2CSelectLowClockSpeed(); // Set to slow
  }
  return true;
}


void post_I2C_by_taskIndex(taskIndex_t taskIndex, deviceIndex_t DeviceIndex) {
  if (!validTaskIndex(taskIndex) || !validDeviceIndex(DeviceIndex)) {
    return;
  }
  if (Device[DeviceIndex].Type != DEVICE_TYPE_I2C) {
    return;
  }
  #if FEATURE_I2CMULTIPLEXER
  I2CMultiplexerOff();
  #endif // if FEATURE_I2CMULTIPLEXER

  I2CSelectHighClockSpeed();  // Reset
}

// Add an event to the event queue.
// event value 1 = taskIndex (first task = 1)
// event value 2 = return value of the plugin function
// Example:  TaskInit#bme=1,0    (taskindex = 0, return value = 0)
void queueTaskEvent(const String& eventName, taskIndex_t taskIndex, const String& value_str) {
  if (Settings.UseRules) {
    String event;
    event.reserve(eventName.length() + 32 + value_str.length());
    event  = eventName;
    event += '#';
    event += getTaskDeviceName(taskIndex);
    event += '=';
    event += taskIndex + 1;
    if (value_str.length() > 0) {
      event += ',';
      event += wrapWithQuotesIfContainsParameterSeparatorChar(value_str);
    }
    eventQueue.addMove(std::move(event));
  }
}

void queueTaskEvent(const String& eventName, taskIndex_t taskIndex, const int& value1) {
  queueTaskEvent(eventName, taskIndex, String(value1));
}

void queueTaskEvent(const __FlashStringHelper * eventName, taskIndex_t taskIndex, const String& value1) {
  queueTaskEvent(String(eventName), taskIndex, value1);
}

void queueTaskEvent(const __FlashStringHelper * eventName, taskIndex_t taskIndex, const int& value1) {
  queueTaskEvent(String(eventName), taskIndex, String(value1));
}

void loadDefaultTaskValueNames_ifEmpty(taskIndex_t TaskIndex) {
  String  oldNames[VARS_PER_TASK];
  uint8_t oldNrDec[VARS_PER_TASK];
  LoadTaskSettings(TaskIndex);

  for (uint8_t i = 0; i < VARS_PER_TASK; ++i) {
    oldNames[i] = ExtraTaskSettings.TaskDeviceValueNames[i];
    oldNrDec[i] = ExtraTaskSettings.TaskDeviceValueDecimals[i];
    oldNames[i].trim();
  }

  struct EventStruct TempEvent(TaskIndex);
  String dummy;
  // the plugin call should populate ExtraTaskSettings with its default values.
  PluginCall(PLUGIN_GET_DEVICEVALUENAMES, &TempEvent, dummy);

  // Restore the settings that were already set by the user
  for (uint8_t i = 0; i < VARS_PER_TASK; ++i) {
    const bool isDefault = oldNames[i].isEmpty();
    ExtraTaskSettings.isDefaultTaskVarName(i, isDefault);
    if (!isDefault) {
      ExtraTaskSettings.setTaskDeviceValueName(i, oldNames[i]);
      ExtraTaskSettings.TaskDeviceValueDecimals[i] = oldNrDec[i];
    }
  }
}

/**
 * Call the plugin of 1 task for 1 function, with standard EventStruct and optional command string
 */
bool PluginCallForTask(taskIndex_t taskIndex, uint8_t Function, EventStruct *TempEvent, String& command, EventStruct *event = nullptr) {
  #ifdef USE_SECOND_HEAP
  HeapSelectDram ephemeral;
  #endif

  bool retval = false;
  const bool considerTaskEnabled = Settings.TaskDeviceEnabled[taskIndex];
   //|| (Settings.TaskDeviceEnabled[taskIndex].enabled && Function == PLUGIN_INIT);

  if (considerTaskEnabled && validPluginID_fullcheck(Settings.getPluginID_for_task(taskIndex)))
  {
    const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(taskIndex);
    if (validDeviceIndex(DeviceIndex)) {
      if (Settings.TaskDeviceDataFeed[taskIndex] == 0) // these calls only to tasks with local feed
      {
        if (Function == PLUGIN_INIT) {
          LoadTaskSettings(taskIndex);
        }
        TempEvent->setTaskIndex(taskIndex);
        TempEvent->sensorType   = Device[DeviceIndex].VType;
        if (event != nullptr) {
          TempEvent->OriginTaskIndex = event->TaskIndex;
        }

        if (!prepare_I2C_by_taskIndex(taskIndex, DeviceIndex)) {
          return false;
        }
        #ifndef BUILD_NO_RAM_TRACKER
        switch (Function) {
          case PLUGIN_WRITE:          // First set
          case PLUGIN_REQUEST:
          case PLUGIN_ONCE_A_SECOND:  // Second set
          case PLUGIN_TEN_PER_SECOND:
          case PLUGIN_FIFTY_PER_SECOND:
          case PLUGIN_INIT:           // Second set, instead of PLUGIN_INIT_ALL
          case PLUGIN_CLOCK_IN:
          case PLUGIN_TIME_CHANGE:
          #if FEATURE_PLUGIN_PRIORITY
          case PLUGIN_PRIORITY_INIT:
          #endif // if FEATURE_PLUGIN_PRIORITY
            {
              checkRAM(F("PluginCall_s"), taskIndex);
              break;
            }
        }
        #endif
        #if FEATURE_I2C_DEVICE_CHECK
        bool i2cStatusOk = true;
        if ((Function == PLUGIN_INIT) && (Device[DeviceIndex].Type == DEVICE_TYPE_I2C) && !Device[DeviceIndex].I2CNoDeviceCheck) {
          const uint8_t i2cAddr = getTaskI2CAddress(taskIndex);
          if (i2cAddr > 0) {
            START_TIMER;
            i2cStatusOk = I2C_deviceCheck(i2cAddr);
            STOP_TIMER_TASK(DeviceIndex, PLUGIN_I2C_GET_ADDRESS);
          }
        }
        if (i2cStatusOk) {
        #endif // if FEATURE_I2C_DEVICE_CHECK
          #ifndef BUILD_NO_RAM_TRACKER
          switch (Function) {
            case PLUGIN_WRITE:          // First set
            case PLUGIN_REQUEST:
            case PLUGIN_ONCE_A_SECOND:  // Second set
            case PLUGIN_TEN_PER_SECOND:
            case PLUGIN_FIFTY_PER_SECOND:
            case PLUGIN_INIT:           // Second set, instead of PLUGIN_INIT_ALL
            case PLUGIN_CLOCK_IN:
            case PLUGIN_TIME_CHANGE:
              {
                checkRAM(F("PluginCall_s"), taskIndex);
                break;
              }
          }
          #endif
          if (Function == PLUGIN_INIT) {
            // Schedule the plugin to be read.
            // Do this before actual init, to allow the plugin to schedule a specific first read.
            Scheduler.schedule_task_device_timer_at_init(TempEvent->TaskIndex);
          }

          START_TIMER;
          retval = (PluginCall(DeviceIndex, Function, TempEvent, command));
          STOP_TIMER_TASK(DeviceIndex, Function);

          if (Function == PLUGIN_INIT) {
            #if FEATURE_PLUGIN_STATS
            if (Device[DeviceIndex].PluginStats) {
              PluginTaskData_base *taskData = getPluginTaskData(taskIndex);
              if (taskData == nullptr) {
                // Plugin apparently does not have PluginTaskData.
                // Create Plugin Task data if it has "Stats" checked.
                LoadTaskSettings(taskIndex);
                if (ExtraTaskSettings.anyEnabledPluginStats()) {
                  initPluginTaskData(taskIndex, new (std::nothrow) _StatsOnly_data_struct());
                }
              }
            }
            #endif // if FEATURE_PLUGIN_STATS
            queueTaskEvent(F("TaskInit"), taskIndex, retval);
          }
        #if FEATURE_I2C_DEVICE_CHECK
        }
        #endif // if FEATURE_I2C_DEVICE_CHECK

        post_I2C_by_taskIndex(taskIndex, DeviceIndex);
        delay(0); // SMY: call delay(0) unconditionally
      } else {
        #if FEATURE_PLUGIN_STATS
        if (Function == PLUGIN_INIT && Device[DeviceIndex].PluginStats) {
          PluginTaskData_base *taskData = getPluginTaskData(taskIndex);
          if (taskData == nullptr) {
            // Plugin apparently does not have PluginTaskData.
            // Create Plugin Task data if it has "Stats" checked.
            LoadTaskSettings(taskIndex);
            if (ExtraTaskSettings.anyEnabledPluginStats()) {
              initPluginTaskData(taskIndex, new (std::nothrow) _StatsOnly_data_struct());
            }
          }
        }
        #endif // if FEATURE_PLUGIN_STATS
      }
    }
  }
  return retval;
}

/*********************************************************************************************\
* Function call to all or specific plugins
\*********************************************************************************************/
bool PluginCall(uint8_t Function, struct EventStruct *event, String& str)
{
  #ifdef USE_SECOND_HEAP
  HeapSelectDram ephemeral;
  #endif

  struct EventStruct TempEvent;

  if (event == nullptr) {
    event = &TempEvent;
  }
  else {
    TempEvent.deep_copy(*event);
  }

  #ifndef BUILD_NO_RAM_TRACKER
  checkRAM(F("PluginCall"), Function);
  #endif

  switch (Function)
  {
    // Unconditional calls to all plugins
    // FIXME TD-er: PLUGIN_UNCONDITIONAL_POLL is not being used at the moment
    /*
    case PLUGIN_UNCONDITIONAL_POLL:    
    {

      const unsigned maxDeviceIndex = getNrBuiltInDeviceIndex();

      for (deviceIndex_t x; x < maxDeviceIndex; ++x) {
          START_TIMER;
          PluginCall(x, Function, event, str);
          STOP_TIMER_TASK(x, Function);
          delay(0); // SMY: call delay(0) unconditionally
      }
      return true;
    }
    */

    case PLUGIN_MONITOR:

      for (auto it = globalMapPortStatus.begin(); it != globalMapPortStatus.end(); ++it) {
        // only call monitor function if there the need to
        if (it->second.monitor || it->second.command || it->second.init) {
          TempEvent.Par1 = getPortFromKey(it->first);

          // initialize the "x" variable to synch with the pluginNumber if second.x == -1
          if (!validDeviceIndex(it->second.x)) { it->second.x = getDeviceIndex(getPluginFromKey(it->first)); }

          const deviceIndex_t DeviceIndex = it->second.x;
          if (validDeviceIndex(DeviceIndex))  {
            START_TIMER;
            PluginCall(DeviceIndex, Function, &TempEvent, str);
            STOP_TIMER_TASK(DeviceIndex, Function);
          }
        }
      }
      return true;


    // Call to all plugins. Return at first match
    case PLUGIN_WRITE:
//    case PLUGIN_REQUEST: @giig1967g: replaced by new function getGPIOPluginValues()
    {
      taskIndex_t firstTask = 0;
      taskIndex_t lastTask = TASKS_MAX;
      String command = String(str);                           // Local copy to avoid warning in ExecuteCommand
      int dotPos = command.indexOf('.');                      // Find first period
      if (Function == PLUGIN_WRITE                            // Only applicable on PLUGIN_WRITE function
        && dotPos > -1) {                                     // First precondition is just a quick check for a period (fail-fast strategy)
        const String arg0 = parseString(command, 1);                // Get first argument
        dotPos = arg0.indexOf('.');
        if (dotPos > -1) {
          String thisTaskName = parseString(arg0, 1, '.');    // Extract taskname prefix
          removeChar(thisTaskName, '[');                      // Remove the optional square brackets
          removeChar(thisTaskName, ']');
          if (thisTaskName.length() > 0) {                    // Second precondition
            taskIndex_t thisTask = findTaskIndexByName(thisTaskName);
            if (!validTaskIndex(thisTask)) {                  // Taskname not found or invalid, check for a task number?
              thisTask = static_cast<taskIndex_t>(atoi(thisTaskName.c_str()));
              if (thisTask == 0 || thisTask > TASKS_MAX) {
                thisTask = INVALID_TASK_INDEX;
              } else {
                thisTask--;                                   // 0-based
              }
            }
            if (validTaskIndex(thisTask)) {                   // Known taskindex?
#ifdef USES_P022                                              // Exclude P022 as it has rather explicit differences in commands when used with the [<TaskName>]. prefix
              const pluginID_t pluginID = Settings.getPluginID_for_task(thisTask);
              if (Settings.TaskDeviceEnabled[thisTask]        // and internally needs to know wether it was called with the taskname prefixed
                && validPluginID_fullcheck(pluginID)
                && Settings.TaskDeviceDataFeed[thisTask] == 0) {
                const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(thisTask);
                constexpr pluginID_t P022_PCA9685_PLUGIN_ID(22);
                if (validDeviceIndex(DeviceIndex) && 
                    pluginID == P022_PCA9685_PLUGIN_ID /* PLUGIN_ID_022 define no longer available, 'assume' 22 for now */) {
                  thisTask = INVALID_TASK_INDEX;
                }
              }
              if (validTaskIndex(thisTask)) {
#endif
                firstTask = thisTask;
                lastTask  = thisTask + 1;                     // Add 1 to satisfy the for condition
                command   = command.substring(dotPos + 1);    // Remove [<TaskName>]. prefix
#ifdef USES_P022
              }
#endif
            }
          }
        }
      }
  // String info = F("PLUGIN_WRITE first: "); // To remove
  // info += firstTask;
  // info += F(" last: ");
  // info += lastTask;
  // addLog(LOG_LEVEL_INFO, info);

      for (taskIndex_t task = firstTask; task < lastTask; task++)
      {
        bool retval = PluginCallForTask(task, Function, &TempEvent, command);

        if (!retval) {
          if (1 == (lastTask - firstTask)) {
            // These plugin task data commands are generic, so only apply them on a specific task.
            // Don't try to match them on the first task that may have such data.
            PluginTaskData_base *taskData = getPluginTaskDataBaseClassOnly(task);
            if (nullptr != taskData) {
              if (taskData->plugin_write_base(event, command)) {
                retval = true;
              }
            }
          }
        }

        if (retval) {
          EventStruct CPlugin_ack_event;
          CPlugin_ack_event.deep_copy(TempEvent);
          CPlugin_ack_event.setTaskIndex(task);
          CPluginCall(CPlugin::Function::CPLUGIN_ACKNOWLEDGE, &CPlugin_ack_event, command);
          return true;
        }
      }

/*
      if (Function == PLUGIN_REQUEST) {
        // @FIXME TD-er: work-around as long as gpio command is still performed in P001_switch.
        for (deviceIndex_t deviceIndex = 0; validDeviceIndex(deviceIndex); deviceIndex++) {
          if (PluginCall(deviceIndex, Function, event, str)) {
            delay(0); // SMY: call delay(0) unconditionally
            CPluginCall(CPlugin::Function::CPLUGIN_ACKNOWLEDGE, event, str);
            return true;
          }
        }
      }
*/
      break;
    }

    // Call to all plugins used in a task. Return at first match
    case PLUGIN_SERIAL_IN:
    case PLUGIN_UDP_IN:
    {
      for (taskIndex_t taskIndex = 0; taskIndex < TASKS_MAX; taskIndex++)
      {
        if (PluginCallForTask(taskIndex, Function, &TempEvent, str)) {
          #ifndef BUILD_NO_RAM_TRACKER
          checkRAM(F("PluginCallUDP"), taskIndex);
          #endif
          return true;
        }
      }
      return false;
    }

    // Call to all plugins that are used in a task
    case PLUGIN_ONCE_A_SECOND:
    case PLUGIN_TEN_PER_SECOND:
    case PLUGIN_FIFTY_PER_SECOND:
    case PLUGIN_INIT_ALL:
    case PLUGIN_CLOCK_IN:
    case PLUGIN_TIME_CHANGE:
    {
      if (Function == PLUGIN_INIT_ALL) {
        Function = PLUGIN_INIT;
      }
      bool result = true;

      for (taskIndex_t taskIndex = 0; taskIndex < TASKS_MAX; taskIndex++)
      {
        #ifndef BUILD_NO_DEBUG
        const int freemem_begin = ESP.getFreeHeap();
        #endif

        bool retval = PluginCallForTask(taskIndex, Function, &TempEvent, str, event);

        if (Function == PLUGIN_INIT) {
          if (!retval && Settings.TaskDeviceDataFeed[taskIndex] == 0) {
            // Disable temporarily as PLUGIN_INIT failed
            // FIXME TD-er: Should reschedule call to PLUGIN_INIT????
            // What interval?  (see: PR #4793)
            //Settings.TaskDeviceEnabled[taskIndex].setRetryInit(); 
            //Scheduler.setPluginTaskTimer(10000, taskIndex, PLUGIN_INIT);
            Settings.TaskDeviceEnabled[taskIndex] = false;
            result = false;
          }
          #ifndef BUILD_NO_DEBUG
          if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
            // See also logMemUsageAfter()
            const int freemem_end = ESP.getFreeHeap();
            String log;
            if (log.reserve(128)) {
              log  = F("After PLUGIN_INIT ");
              log += F(" task: ");
              if (taskIndex < 9) log += ' ';
              log += taskIndex + 1;
              while (log.length() < 30) log += ' ';
              log += F("Free mem after: ");
              log += freemem_end;
              while (log.length() < 53) log += ' ';
              log += F("plugin: ");
              log += freemem_begin - freemem_end;
              while (log.length() < 67) log += ' ';

              log += Settings.TaskDeviceEnabled[taskIndex] ? F("[ena]") : F("[dis]");
              while (log.length() < 73) log += ' ';
              log += getPluginNameFromDeviceIndex(getDeviceIndex_from_TaskIndex(taskIndex));

              addLogMove(LOG_LEVEL_DEBUG, log);
            }
          }
          #endif
        }
      }

      return result;
    }

    #if FEATURE_PLUGIN_PRIORITY
    case PLUGIN_PRIORITY_INIT_ALL:
    {
      if (Function == PLUGIN_PRIORITY_INIT_ALL) {
        addLogMove(LOG_LEVEL_INFO, F("INIT : Check for Priority tasks"));
        PluginInit(true); // Priority only, load plugins but don't initialize them yet
        Function = PLUGIN_PRIORITY_INIT;
      }

      for (taskIndex_t taskIndex = 0; taskIndex < TASKS_MAX; taskIndex++) {
        bool isPriority = PluginCallForTask(taskIndex, Function, &TempEvent, str, event);

        if ((Function == PLUGIN_PRIORITY_INIT) && isPriority) { // If this is a priority task, then initialize it, next PLUGIN_INIT call must be self-ignored by plugin!
          clearPluginTaskData(taskIndex);                       // Make sure any task data is actually cleared.
          if (PluginCallForTask(taskIndex, PLUGIN_INIT, &TempEvent, str, event) &&
              loglevelActiveFor(LOG_LEVEL_INFO)) {
            String log;
            log.reserve(80);
            log += concat(F("INIT : Started Priority task "), static_cast<int>(taskIndex + 1));
            log += F(", [");
            log += getTaskDeviceName(taskIndex);
            log += F("] ");
            log += getPluginNameFromDeviceIndex(getDeviceIndex_from_TaskIndex(taskIndex));
            addLogMove(LOG_LEVEL_INFO, log);
          }
        }
      }

      return true;
    }
    #endif // if FEATURE_PLUGIN_PRIORITY

    // Call to specific task which may interact with the hardware
    case PLUGIN_INIT:
    case PLUGIN_EXIT:
    case PLUGIN_WEBFORM_LOAD:
    case PLUGIN_WEBFORM_LOAD_ALWAYS:
    case PLUGIN_WEBFORM_LOAD_OUTPUT_SELECTOR:
    case PLUGIN_READ:
    case PLUGIN_GET_PACKED_RAW_DATA:
    case PLUGIN_TASKTIMER_IN:
    case PLUGIN_PROCESS_CONTROLLER_DATA:
    {
      // FIXME TD-er: Code duplication with PluginCallForTask
      if (!validTaskIndex(event->TaskIndex)) {
        return false;
      }
      if (Function == PLUGIN_READ || Function == PLUGIN_INIT || Function == PLUGIN_PROCESS_CONTROLLER_DATA) {
        if (!Settings.TaskDeviceEnabled[event->TaskIndex]) {
          return false;
        }
      }
      const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(event->TaskIndex);

      if (validDeviceIndex(DeviceIndex)) {
        if (ExtraTaskSettings.TaskIndex != event->TaskIndex) {
          if (Function == PLUGIN_READ && Device[DeviceIndex].ErrorStateValues) {
            // PLUGIN_READ should not need to access ExtraTaskSettings except for what's already being cached.
            // Only exception is when ErrorStateValues is needed.
            // Therefore only need to call LoadTaskSettings for those tasks with ErrorStateValues
            LoadTaskSettings(event->TaskIndex);
          } else if (Function == PLUGIN_INIT || Function == PLUGIN_WEBFORM_LOAD || Function == PLUGIN_WEBFORM_LOAD_ALWAYS) {
            // LoadTaskSettings may call PLUGIN_GET_DEVICEVALUENAMES.
            LoadTaskSettings(event->TaskIndex);
          }
        }
        event->BaseVarIndex = event->TaskIndex * VARS_PER_TASK;

        #ifndef BUILD_NO_RAM_TRACKER
        checkRAM_PluginCall_task(event->TaskIndex, Function);
        #endif

        if (!prepare_I2C_by_taskIndex(event->TaskIndex, DeviceIndex)) {
          return false;
        }
        bool retval = false;
        const bool performPluginCall = 
          (Function != PLUGIN_READ) || 
          (Settings.TaskDeviceDataFeed[event->TaskIndex] == 0);
        #if FEATURE_I2C_DEVICE_CHECK
        bool i2cStatusOk = true;
        if (Settings.TaskDeviceDataFeed[event->TaskIndex] == 0) {
          // Only for locally connected sensors, not virtual ones via p2p.
          if (((Function == PLUGIN_INIT) || (Function == PLUGIN_READ))
              && (Device[DeviceIndex].Type == DEVICE_TYPE_I2C) && !Device[DeviceIndex].I2CNoDeviceCheck) {
            const uint8_t i2cAddr = getTaskI2CAddress(event->TaskIndex);
            if (i2cAddr > 0) {
              START_TIMER;
              // Disable task when device is unreachable for 10 PLUGIN_READs or 1 PLUGIN_INIT
              i2cStatusOk = I2C_deviceCheck(i2cAddr, event->TaskIndex, Function == PLUGIN_INIT ? 1 : 10);
              STOP_TIMER_TASK(DeviceIndex, PLUGIN_I2C_GET_ADDRESS);
            }
          }
        }
        if (i2cStatusOk) {
        #endif // if FEATURE_I2C_DEVICE_CHECK
          START_TIMER;

          if (((Function == PLUGIN_INIT) ||
              (Function == PLUGIN_WEBFORM_LOAD)) &&
              Device[DeviceIndex].ErrorStateValues) { // Only when we support ErrorStateValues
            // FIXME TD-er: Not sure if this should be called here.
            // It may be better if ranges are set in the call for default values and error values set via PLUGIN_INIT.
            // Also these may be plugin specific so perhaps create a helper function to load/save these values and call these helpers from the plugin code.
            PluginCall(DeviceIndex, PLUGIN_INIT_VALUE_RANGES, event, str); // Initialize value range(s)
          }

          if ((Function == PLUGIN_INIT)
              #if FEATURE_PLUGIN_PRIORITY
              && !Settings.isPriorityTask(event->TaskIndex) // Don't clear already initialized PriorityTask data
              #endif // if FEATURE_PLUGIN_PRIORITY
             ) {
            // Make sure any task data is actually cleared.
            clearPluginTaskData(event->TaskIndex);
          /*
            #if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
            if (Device[DeviceIndex].isSerial()) {
              checkSerialConflict(
                serialHelper_getSerialType(event),
                serialHelper_getRxPin(event),
                serialHelper_getTxPin(event));
            }
            #endif
          */
          }

          if (performPluginCall) {
            retval = PluginCall(DeviceIndex, Function, event, str);
          } else {
            retval = event->Source == EventValueSource::Enum::VALUE_SOURCE_UDP;
          }

          if (Function == PLUGIN_READ) {
            if (!retval) {
              String errorStr;
              if (PluginCall(DeviceIndex, PLUGIN_READ_ERROR_OCCURED, event, errorStr))
              {
                // Apparently the last read call resulted in an error
                // Send event indicating the error.
                queueTaskEvent(F("TaskError"), event->TaskIndex, errorStr);
              }
            } else {
              #if FEATURE_PLUGIN_STATS
              PluginTaskData_base *taskData = getPluginTaskDataBaseClassOnly(event->TaskIndex);
              if (taskData != nullptr) {
                taskData->pushPluginStatsValues(event, !Device[DeviceIndex].TaskLogsOwnPeaks);
              }
              #endif // if FEATURE_PLUGIN_STATS
              saveUserVarToRTC();
            }
          }
          if (Function == PLUGIN_INIT) {
            if (!retval && Settings.TaskDeviceDataFeed[event->TaskIndex] == 0) {
              // Disable temporarily as PLUGIN_INIT failed
              // FIXME TD-er: Should reschedule call to PLUGIN_INIT????
              Settings.TaskDeviceEnabled[event->TaskIndex] = false;
            } else {
              #if FEATURE_PLUGIN_STATS
              if (Device[DeviceIndex].PluginStats) {
                PluginTaskData_base *taskData = getPluginTaskData(event->TaskIndex);
                if (taskData == nullptr) {
                  // Plugin apparently does not have PluginTaskData.
                  // Create Plugin Task data if it has "Stats" checked.
                  LoadTaskSettings(event->TaskIndex);
                  if (ExtraTaskSettings.anyEnabledPluginStats()) {
                    initPluginTaskData(event->TaskIndex, new (std::nothrow) _StatsOnly_data_struct());
                  }
                }
              }
              #endif // if FEATURE_PLUGIN_STATS
              // Schedule the plugin to be read.
              Scheduler.schedule_task_device_timer_at_init(TempEvent.TaskIndex);
              queueTaskEvent(F("TaskInit"), event->TaskIndex, retval);
            }
          }
          if (Function == PLUGIN_EXIT) {
            clearPluginTaskData(event->TaskIndex);
//            initSerial();
            queueTaskEvent(F("TaskExit"), event->TaskIndex, retval);
            updateActiveTaskUseSerial0();
          }
          STOP_TIMER_TASK(DeviceIndex, Function);
        #if FEATURE_I2C_DEVICE_CHECK
        }
        #endif // if FEATURE_I2C_DEVICE_CHECK
        post_I2C_by_taskIndex(event->TaskIndex, DeviceIndex);
        delay(0); // SMY: call delay(0) unconditionally

        return retval;
      }
      return false;
    }

    // Call to specific task not interacting with hardware
    case PLUGIN_GET_CONFIG_VALUE:
    case PLUGIN_GET_DEVICEVALUENAMES:
    case PLUGIN_GET_DEVICEGPIONAMES:
    case PLUGIN_WEBFORM_SAVE:
    case PLUGIN_WEBFORM_SHOW_VALUES:
    case PLUGIN_WEBFORM_SHOW_CONFIG:
    case PLUGIN_WEBFORM_SHOW_I2C_PARAMS:
    case PLUGIN_WEBFORM_SHOW_SERIAL_PARAMS:
    case PLUGIN_WEBFORM_SHOW_GPIO_DESCR:
    #if FEATURE_PLUGIN_STATS
    case PLUGIN_WEBFORM_LOAD_SHOW_STATS:
    #endif // if FEATURE_PLUGIN_STATS
    case PLUGIN_SET_CONFIG:
    case PLUGIN_SET_DEFAULTS:
    case PLUGIN_I2C_HAS_ADDRESS:
    case PLUGIN_WEBFORM_SHOW_ERRORSTATE_OPT:
    case PLUGIN_INIT_VALUE_RANGES:

    // PLUGIN_MQTT_xxx functions are directly called from the scheduler.
    //case PLUGIN_MQTT_CONNECTION_STATE:
    //case PLUGIN_MQTT_IMPORT:
    {
      START_TIMER;
      const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(event->TaskIndex);

      if (validDeviceIndex(DeviceIndex)) {
        if (Function == PLUGIN_GET_DEVICEVALUENAMES ||
            Function == PLUGIN_WEBFORM_SAVE ||
            Function == PLUGIN_WEBFORM_LOAD ||
            Function == PLUGIN_WEBFORM_LOAD_ALWAYS ||
            Function == PLUGIN_SET_DEFAULTS ||
            Function == PLUGIN_INIT_VALUE_RANGES ||
            Function == PLUGIN_WEBFORM_SHOW_SERIAL_PARAMS
        ) {
          LoadTaskSettings(event->TaskIndex);
        }
        event->BaseVarIndex = event->TaskIndex * VARS_PER_TASK;

        #ifndef BUILD_NO_RAM_TRACKER
        checkRAM_PluginCall_task(event->TaskIndex, Function);
        #endif

        if (Function == PLUGIN_SET_DEFAULTS) {
          for (int i = 0; i < VARS_PER_TASK; ++i) {
            UserVar[event->BaseVarIndex + i] = 0.0f;
          }
        }

        bool retval =  PluginCall(DeviceIndex, Function, event, str);

        // Calls may have updated ExtraTaskSettings, so validate them.
        ExtraTaskSettings.validate();

        if (Function == PLUGIN_GET_DEVICEVALUENAMES ||
            Function == PLUGIN_WEBFORM_SAVE ||
            Function == PLUGIN_SET_DEFAULTS ||
            Function == PLUGIN_INIT_VALUE_RANGES ||
           (Function == PLUGIN_SET_CONFIG && retval)) {
          // Each of these may update ExtraTaskSettings, but it may not have been saved yet.
          // Thus update the cache just in case something from it is requested from the cache.
          Cache.updateExtraTaskSettingsCache();
        }
        if (Function == PLUGIN_SET_DEFAULTS) {
          saveUserVarToRTC();
        }
        if (Function == PLUGIN_GET_CONFIG_VALUE && !retval) {
          // Try to match a statistical property of a task value.
          // e.g.: [taskname#valuename.avg]
          PluginTaskData_base *taskData = getPluginTaskDataBaseClassOnly(event->TaskIndex);
          if (nullptr != taskData) {
            if (taskData->plugin_get_config_value_base(event, str)) {
              retval = true;
            }
          }
        }

        
        STOP_TIMER_TASK(DeviceIndex, Function);
        delay(0); // SMY: call delay(0) unconditionally
        return retval;
      }
      return false;
    }

    // Frequently made call to specific task not interacting with hardware
    case PLUGIN_GET_DEVICEVALUECOUNT:
    case PLUGIN_GET_DEVICEVTYPE:
    case PLUGIN_FORMAT_USERVAR:
    {
      START_TIMER;
      const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(event->TaskIndex);

      if (validDeviceIndex(DeviceIndex)) {
        event->BaseVarIndex = event->TaskIndex * VARS_PER_TASK;

        #ifndef BUILD_NO_RAM_TRACKER
//        checkRAM_PluginCall_task(event->TaskIndex, Function);
        #endif

        if (Function == PLUGIN_GET_DEVICEVALUECOUNT) {
          event->Par1 = Device[DeviceIndex].ValueCount;
        }
        if (Function == PLUGIN_GET_DEVICEVTYPE) {
          event->sensorType = Device[DeviceIndex].VType;
        }
        bool retval =  PluginCall(DeviceIndex, Function, event, str);
        if (Function == PLUGIN_GET_DEVICEVALUECOUNT) {
          // Check if we have a valid value count.
          if (Output_Data_type_t::Simple == Device[DeviceIndex].OutputDataType) {
            if (event->Par1 < 1 || event->Par1 > VARS_PER_TASK) {
              // Output_Data_type_t::Simple only allows for 1 .. 4 output types.
              // Apparently the value is not correct, so use the default.
              event->Par1 = Device[DeviceIndex].ValueCount;
            }
          }
        }
        STOP_TIMER_TASK(DeviceIndex, Function);
        delay(0); // SMY: call delay(0) unconditionally
        return retval;
      }
      return false;
    }


  } // case
  return false;
}

