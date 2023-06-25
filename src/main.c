#include <stdio.h>

#include <app/app.h>

int main(int argc, char**argv){
    for(int i=0;i<argc;i++){
        printf("got arg: %s\n",argv[i]);
    }

    Application *app=App_new();

    App_set_window_title(app,"my window");

    App_run(app);

    App_destroy(app);
    
    return 0;
}
