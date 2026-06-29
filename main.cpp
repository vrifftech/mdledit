#if defined(UNICODE) && !defined(_UNICODE)
    #define _UNICODE
#elif defined(_UNICODE) && !defined(UNICODE)
    #define UNICODE
#endif

#include "frame.h"

int WINAPI WinMain(HINSTANCE hThisInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR lpszArgument, /// This string contains any CLI arguments that were used to run the .exe
                     int nCmdShow){
    (void)hPrevInstance;

    try{
        MSG messages;            /* Here messages to the application are saved */

        const std::string sArguments = lpszArgument != nullptr ? std::string(lpszArgument) : std::string();
        if(!sArguments.empty()){
            Warning("You have run mdledit with arguments, but this is not supported yet!\n\n"
                    "These are the arguments: " + sArguments);
        }

        //Creation of window
        std::unique_ptr<Frame> winMain(new Frame(hThisInstance));
        if(!winMain->Run(nCmdShow)){
            return 1; // error
        }

        /* Run the message loop. It will run until GetMessage() returns 0 */
        while(GetMessage (&messages, NULL, 0, 0)){

            /* Translate virtual-key messages into character messages */
            TranslateMessage(&messages);
            /* Send message to WindowProcedure */
            DispatchMessage(&messages);
        }
        //Will only get this far after while is terminated

        //Gdiplus::GdiplusShutdown(gdiplusToken);

        /* The program return-value is 0 - The value that PostQuitMessage() gave */
        return static_cast<int>(messages.wParam);
    }
    catch(const std::exception & e){
        Error(std::string("Unhandled exception:\n\n") + e.what());
        std::cout << "Unhandled exception:\n" << e.what() << "\n";
        return 1;
    }
    catch(...){
        Error("Unhandled unknown exception.");
        std::cout << "Unhandled unknown exception.\n";
        return 1;
    }
}
