#include "gamepad.h"
SDL_GameController* maingamepad;
FVARP(gamepad_sensitivity, 1e-3f, 3.0f, 1000.0f);
VARP(gamepad_lookstick, 0, 1, 1);
VARP(gamepad_deadzone, 0, 1000, 32760);

VARP(gamepad_yinv, 0, 0, 1);                        // invert y-axis movement (if "1")
FVARP(gamepad_lookaccel, 0, 0, 1000);                  // make fast movement even faster (zero deactivates the feature)
FVARP(gamepad_filter, 0.0f, 0.0f, 6.0f);               // simple lowpass filtering (zero deactivates the feature)

extern float sensitivityscale;
extern int autoscopesens;
extern float autoscopesensscale;
extern float scopesensscale;
extern float scopesens;

int gamepad_opencontroller(int which) {
    maingamepad = SDL_GameControllerOpen( which );
    
    if( maingamepad == NULL ) {
        conoutf("\f3Warning: \f5Unable to open game controller! SDL Error: %s", SDL_GetError());
        return 1;
    } else {
        conoutf("Connected gamepad: %s", SDL_GameControllerName(maingamepad));
        return 0;
    }
}

void gamepad_openavaliablecontroller() {
    //Check for joysticks
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            if (!gamepad_opencontroller(i)) break;
        }
    }
}


void gamepad_init() {
    //Load mappings
    const char *mappingfile = findfile("config/gamecontrollerdb.txt", "rb");
    conoutf("Loading SDL Controller mappings from %s", mappingfile);
    if ( SDL_GameControllerAddMappingsFromFile(mappingfile) == -1 ) {
        conoutf("Warning: Unable to open game controller mapping file! SDL Error: %s", SDL_GetError());
    }
    
    gamepad_openavaliablecontroller();
}

void gamepad_quit() {
    if (maingamepad) {
        SDL_GameControllerClose( maingamepad );
    }
    
    maingamepad = NULL;
}


void gamepad_controlleradded(int which) {
    if (maingamepad != NULL) return;
    gamepad_opencontroller(which);
}

void gamepad_controllerremoved(int which) {
    if (maingamepad == NULL) return;
    
    if ( SDL_GameControllerFromInstanceID(which) == maingamepad ) {
        SDL_GameControllerClose(maingamepad);
        maingamepad = NULL;
        gamepad_openavaliablecontroller();
    }
}

void gamepad_power() {
    if (maingamepad == NULL) {
        intret(-1);
        return;
    }
    SDL_Joystick* joy = SDL_GameControllerGetJoystick(maingamepad);
    intret(SDL_JoystickCurrentPowerLevel(joy));
}
COMMAND(gamepad_power, "");

void gamepad_list() {
    vector<char> w;
    w.add('a');
    w.add('b');
    w.add('c');
    resultcharvector(w, 1);
}
COMMAND(gamepad_list, "");


void gamepad_look() {
    if (maingamepad == NULL) return;
    
    if(intermission ||  (player1->isspectating() && player1->spectatemode==SM_FOLLOW1ST)) return;
    bool zooming = player1->weaponsel->type == GUN_SNIPER && ((sniperrifle *)player1->weaponsel)->scoped;               // check if player uses scope
    
    SDL_GameControllerAxis axis_x = SDL_CONTROLLER_AXIS_LEFTX;
    SDL_GameControllerAxis axis_y = SDL_CONTROLLER_AXIS_LEFTY;
    
    if (gamepad_lookstick == 1) {
        axis_x = SDL_CONTROLLER_AXIS_RIGHTX;
        axis_y = SDL_CONTROLLER_AXIS_RIGHTY;
    }
    
    float dx = 0, dy = 0;
    int raw_x = SDL_GameControllerGetAxis(maingamepad, axis_x);
    int raw_y = SDL_GameControllerGetAxis(maingamepad, axis_y);
    
    if ( abs(raw_x) > gamepad_deadzone) {
        dx = (float(raw_x - gamepad_deadzone) / 32768);
    }
    
    if ( abs(raw_y) > gamepad_deadzone) {
        dy = (float(raw_y - gamepad_deadzone) / 32768);
    }
    
    if(gamepad_filter > 0.0001f) { // simple IIR-like filter (1st order lowpass)
        static float fdx = 0, fdy = 0;
        float k = gamepad_filter * 0.1f;
        dx = fdx = dx * ( 1.0f - k ) + fdx * k;
        dy = fdy = dy * ( 1.0f - k ) + fdy * k;
    }
    
    double cursens = gamepad_sensitivity;                                                                                     // basic unscoped sensitivity
    if(gamepad_lookaccel > 0.0001f && curtime && ((int)dx || (int)dy)) cursens += 0.02f * gamepad_lookaccel * sqrtf(dx*dx + dy*dy)/curtime;   // optionally accelerated
    if(zooming)
    {                                                                                                                   //      when scoped:
        if(scopesens > 0.0001f) cursens = scopesens;                                                                    //          if specified, use dedicated (fixed) scope sensitivity
        else cursens *= autoscopesens ? autoscopesensscale : scopesensscale;                                            //          or adjust sensitivity by given (fixed) factor or based on scopefov/fov
    }
    cursens /= 33.0f * sensitivityscale;
    
    if (gamepad_yinv) dy *= -1;
    
    camera1->yaw += (float) (dx * cursens);
    camera1->pitch -= (float) (dy * cursens);
    
    fixcamerarange();
    if(camera1!=player1 && player1->spectatemode!=SM_DEATHCAM)
    {
        player1->yaw = camera1->yaw;
        player1->pitch = camera1->pitch;
    }
}

extern char axisstates[SDL_CONTROLLER_AXIS_MAX*2];
void gamepad_debugrender() {
    char text[1000];
    char text2[1000];
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX*2; i++) {
        defformatstring(chunk)(" %d", axisstates[i]);
        strcat(text, chunk);
    }
    
    for (int i = SDL_CONTROLLER_AXIS_LEFTX; i < SDL_CONTROLLER_AXIS_MAX; i++) {
        defformatstring(chunk)(" %d", SDL_GameControllerGetAxis(maingamepad, (SDL_GameControllerAxis) i));
        strcat(text2, chunk);
    }
    
    draw_text(text, 30, 500);
    draw_text(text2, 30, 600);
}
