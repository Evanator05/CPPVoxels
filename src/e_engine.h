// initializes all of the engines systems
void engine_init(void);

// cleans up all of the engines systems
void engine_cleanup(void);

// one frame update of the engine
void engine_update(double deltaTime);

// the main loop of the engine
void engine_loop(void);

// the main entry point to the engine
int engine_main(void);

// quits the game
void engine_quit(void);