#include "src/Helpers/_CPlugin_Helper.h"
#ifdef USES_C003

// #######################################################################################################
// ########################### Controller Plugin 003: Nodo Telnet  #######################################
// #######################################################################################################

# define CPLUGIN_003
# define CPLUGIN_ID_003         3
# define CPLUGIN_NAME_003       "Nodo Telnet"

bool CPlugin_003(CPlugin::Function function, struct EventStruct *event, String& string)
{
  bool success = false;

  switch (function)
  {
    case CPlugin::Function::CPLUGIN_PROTOCOL_ADD:
    {
      ProtocolStruct& proto = getProtocolStruct(event->idx); //      = CPLUGIN_ID_003;
      proto.usesMQTT     = false;
      proto.usesAccount  = false;
      proto.usesPassword = true;
      proto.defaultPort  = 23;
      proto.usesID       = true;
      break;
    }

    case CPlugin::Function::CPLUGIN_GET_DEVICENAME:
    {
      string = F(CPLUGIN_NAME_003);
      break;
    }

    case CPlugin::Function::CPLUGIN_INIT:
    {
      success = init_c003_delay_queue(event->ControllerIndex);
      break;
    }

    case CPlugin::Function::CPLUGIN_EXIT:
    {
      exit_c003_delay_queue();
      break;
    }

    case CPlugin::Function::CPLUGIN_PROTOCOL_SEND:
    {
      if (C003_DelayHandler == nullptr) {
        break;
      }
      if (C003_DelayHandler->queueFull(event->ControllerIndex)) {
        break;
      }


      // We now create a URI for the request
      String url = F("variableset ");
      url    += event->idx;
      url    += ',';
      url    += formatUserVarNoCheck(event, 0);
      url    += '\n';

      std::unique_ptr<C003_queue_element> element(new C003_queue_element(event->ControllerIndex, event->TaskIndex, std::move(url)));

      success = C003_DelayHandler->addToQueue(std::move(element));
      Scheduler.scheduleNextDelayQueue(SchedulerIntervalTimer_e::TIMER_C003_DELAY_QUEUE, C003_DelayHandler->getNextScheduleTime());

      break;
    }

    case CPlugin::Function::CPLUGIN_FLUSH:
    {
      process_c003_delay_queue();
      delay(0);
      break;
    }

    default:
      break;
  }
  return success;
}

// Uncrustify may change this into multi line, which will result in failed builds
// *INDENT-OFF*
bool do_process_c003_delay_queue(int controller_number, const Queue_element_base& element_base, ControllerSettingsStruct& ControllerSettings) {
  const C003_queue_element& element = static_cast<const C003_queue_element&>(element_base);
// *INDENT-ON*
  bool success = false;

  // Use WiFiClient class to create TCP connections
  WiFiClient client;

  if (!try_connect_host(controller_number, client, ControllerSettings, F("TELNT: ")))
  {
    return success;
  }

  // strcpy_P(log, PSTR("TELNT: Sending enter"));
  // addLog(LOG_LEVEL_ERROR, log);
  client.print(" \n");

  unsigned long timer = millis() + 200;

  while (!client_available(client) && !timeOutReached(timer)) {
    delay(1);
  }

  timer = millis() + 1000;

  while (client_available(client) && !timeOutReached(timer) && !success)
  {
    //   String line = client.readStringUntil('\n');
    String line;
    safeReadStringUntil(client, line, '\n');

    if (line.startsWith(F("Enter your password:")))
    {
      success = true;
      #ifndef BUILD_NO_DEBUG
      addLog(LOG_LEVEL_DEBUG, F("TELNT: Password request ok"));
      #endif
    }
    delay(1);
  }
  #ifndef BUILD_NO_DEBUG
  addLog(LOG_LEVEL_DEBUG, F("TELNT: Sending pw"));
  #endif
  client.println(getControllerPass(element._controller_idx, ControllerSettings));
  delay(100);

  while (client_available(client)) {
    client.read();
  }

  #ifndef BUILD_NO_DEBUG
  addLog(LOG_LEVEL_DEBUG, F("TELNT: Sending cmd"));
  #endif
  client.print(element.txt);
  delay(10);

  while (client_available(client)) {
    client.read();
  }

  #ifndef BUILD_NO_DEBUG
  addLog(LOG_LEVEL_DEBUG, F("TELNT: closing connection"));
  #endif

  client.stop();
  return success;
}

#endif // ifdef USES_C003
