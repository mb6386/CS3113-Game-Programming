//Platformer Game
//Mark Bekker and Kenny Yip


#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <vector>
#include "ShaderProgram.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "FlareMap.hpp"
#include "Entity.hpp"

#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6

using namespace std;

glm::mat4 projectionMatrix = glm::mat4(1.0f);
SDL_Event event;
bool done = false;
glm::mat4 modelMatrix = glm::mat4(1.0f);
glm::mat4 viewMatrix = glm::mat4(1.0f);
float lastFrameTicks = 0.0f;
ShaderProgram program2;
float elapsed;
GLuint spriteSheetTexture;

bool win = false; //when win condition is reached
int keyCount = 0;
int keyID = 86;


ShaderProgram program;
vector<Entity> entities;
GLuint fontTexture;
enum GameMode {STATE_MAIN_MENU, STATE_GAME_LEVEL};
GameMode mode = STATE_MAIN_MENU;
vector<Entity> enemies;

vector<Entity> levelkeys;


vector<float> vertexData;
vector<float> texCoordData;
vector<int> palpableBlockIds = {33,34,35,96,97,32,17,100}; //IDs of collidable blocks
int spikeID = 100;
int sprite_count_x = 16;
int sprite_count_y = 8;
float tileSize = 0.15;
float scale = 0.1;
FlareMap map;
vector<int> palpables;
float gravity = 0.0075f;
float accumulator = 0.0f;
bool JumpOn = false;

int currentGameLevel = 1;
SDL_Window* displayWindow;

GLuint LoadTexture(const char *filePath) {
    int w,h,comp;
    unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
    if(image == NULL) {
        cout << "Unable to load image. Make sure the path is correct\n";
        assert(false);
    }
    GLuint retTexture;
    glGenTextures(1, &retTexture);
    glBindTexture(GL_TEXTURE_2D, retTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(image);
    return retTexture;
}



void DrawText(ShaderProgram &program, int fontTexture, string text, float x, float y, float size, float spacing) {
    float text_size = 1.0 / 16.0f;
    vector<float> vertexData;
    vector<float> texCoordData;
    glm::mat4 textModelMatrix = glm::mat4(1.0f);
    textModelMatrix = glm::translate(textModelMatrix, glm::vec3(x, y, 1.0f));
    for (size_t i = 0; i < text.size(); i++) {
        float text_x = ((text[i]) % 16) / 16.0f;
        float text_y = ((text[i]) / 16) / 16.0f;
        vertexData.insert(vertexData.end(), {
            ((size + spacing) * i) + (-0.5f * size), 0.5f * size,
            ((size + spacing) * i) + (-0.5f * size), -0.5f * size,
            ((size + spacing) * i) + (0.5f * size), 0.5f * size,
            ((size + spacing) * i) + (0.5f * size), -0.5f * size,
            ((size + spacing) * i) + (0.5f * size), 0.5f * size,
            ((size + spacing) * i) + (-0.5f * size), -0.5f * size,
        });
        texCoordData.insert(texCoordData.end(), {
            text_x, text_y,
            text_x, text_y + text_size,
            text_x + text_size, text_y,
            text_x + text_size, text_y + text_size,
            text_x + text_size, text_y,
            text_x, text_y + text_size,
        });
    }
    glUseProgram(program.programID);
    program.SetModelMatrix(textModelMatrix);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
    glEnableVertexAttribArray(program.positionAttribute);
    glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
    glEnableVertexAttribArray(program.texCoordAttribute);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glDrawArrays(GL_TRIANGLES, 0, text.size() * 6.0f);
    glDisableVertexAttribArray(program.positionAttribute);
    glDisableVertexAttribArray(program.texCoordAttribute);
}


void drawMap(){
    glUseProgram(program.programID);
    glm::mat4 mapModelMatrix = glm::mat4(1.0);
    program.SetModelMatrix(mapModelMatrix);
    glEnable(GL_BLEND);
    
    glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
    glEnableVertexAttribArray(program.positionAttribute);
    glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
    glEnableVertexAttribArray(program.texCoordAttribute);
    glBindTexture(GL_TEXTURE_2D, spriteSheetTexture);
    glDrawArrays(GL_TRIANGLES, 0, vertexData.size()/2.0f);
    glDisableVertexAttribArray(program.positionAttribute);
    glDisableVertexAttribArray(program.texCoordAttribute);
}


void gameOver(){
    cout << "Game Over" << endl;
}



//Collision Detection
void worldToTileCoordinates(float worldX, float worldY, int *map_X, int *map_Y) {
    *map_X = (int)(worldX / tileSize);
    *map_Y = (int)(worldY / -tileSize);
}

bool playerCollideTop(){ //handle top collision with block
    int map_X = 0;
    int map_Y = 0;
    map_X = (entities[0].position.x / tileSize);
    map_Y = ((entities[0].position.y + (entities[0].height / 2)) / -tileSize);
    
    if(map_X < map.mapWidth && map_Y < map.mapHeight){
        for(int tileID: palpables){
            if(map.mapData[map_Y][map_X] == tileID){
                entities[0].position.y -= fabs(((-tileSize * map_Y) - tileSize) - (entities[0].position.y + entities[0].height/2))+0.001;
                return true;
            }
            if (map.mapData[map_Y][map_X] == spikeID){ //Spikes kill you if you land on it
                gameOver();
            }
            
            if (map.mapData[map_Y][map_X] == keyID){
                map.mapData[map_Y][map_X] = 0;
                cout << "KEY" << endl;
                keyCount ++;
            }
        }
    }
        
        
    entities[0].collidedBottom = false;
    return false;
}

bool playerCollideBottom(){ //handle bottom collision with block
    int map_X = 0;
    int map_Y = 0;
    map_X = (entities[0].position.x / tileSize);
    map_Y = ((entities[0].position.y - (tileSize/ 2)) / -tileSize);
    
    if(map_X < map.mapWidth && map_Y < map.mapHeight){
        for(int tileID: palpables){
            if(map.mapData[map_Y][map_X] == tileID){
                entities[0].collidedBottom = true;
                entities[0].position.y += fabs((-tileSize * map_Y) - (entities[0].position.y - tileSize/2))+0.001;
                JumpOn = true;
                return true;
            }
        }
    }
    entities[0].collidedBottom = false;
    return false;
}


bool playerCollideLeft(){ //handle left collision with block
    int map_X =0;
    int map_Y = 0;
    map_X = ((entities[0].position.x - (entities[0].width / 2))/ tileSize);
    map_Y = (entities[0].position.y / -tileSize);
    
    if(map_X < map.mapWidth && map_Y < map.mapHeight){
        for(int tileID: palpables){
            if(map.mapData[map_Y][map_X] == tileID){
                entities[0].position.x += fabs(((tileSize * map_X) + tileSize) - (entities[0].position.x - tileSize/2))+0.001;
                return true;
            }
        }
    }
    entities[0].collidedBottom = false;
    return false;
}


bool playerCollideRight(){ //handle right collision with block
    int map_X = 0;
    int map_Y = 0;
    map_X = ((entities[0].position.x + (entities[0].width / 2))/ tileSize);
    map_Y = (entities[0].position.y / -tileSize);

    if(map_X < map.mapWidth && map_Y < map.mapHeight){
        for(int tileID: palpables){
            if(map.mapData[map_Y][map_X] == tileID){
                entities[0].position.x -= fabs(((tileSize * map_X) + tileSize) - (entities[0].position.x + tileSize/2))+0.001;
                return true;
            }
        }
    }
    return false;
}







class mainMenu {
public:
    void Render() {
        DrawText(program, fontTexture, "Space to Start", -0.80,0,0.1,0.01);
    }
    
    void Update() {
        if(currentGameLevel == 1){
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        }
        else if(currentGameLevel == 2){
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }
        else if(currentGameLevel == 3){
            glClearColor(0.72f, 0.61f, 1.0f, 1.0f);
        }
    }
    void Events(){
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
            else if (event.type == SDL_KEYDOWN){
                if(event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    mode = STATE_GAME_LEVEL;
                    lastFrameTicks = (float)SDL_GetTicks()/1000.0f;
                }
            }
            
        }
    }
    void Clean(){}
};





class Game{
public:
    void Setup(){
        SDL_Init(SDL_INIT_VIDEO);
        displayWindow = SDL_CreateWindow("Final Project", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL);
        SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
        SDL_GL_MakeCurrent(displayWindow, context);
        glViewport(0, 0, 1280, 720);
        projectionMatrix = glm::ortho(-1.77f,1.77f, -1.0f, 1.0f, -1.0f, 1.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        program.Load("vertex_textured.glsl","fragment_textured.glsl");
        program2.Load("vertex.glsl","fragment.glsl");
        program2.SetViewMatrix(viewMatrix);
        program2.SetProjectionMatrix(projectionMatrix);
        program2.SetModelMatrix(modelMatrix);
        program.SetViewMatrix(viewMatrix);
        program.SetProjectionMatrix(projectionMatrix);
        program.SetModelMatrix(modelMatrix);
        
        //Load textures
        spriteSheetTexture = LoadTexture("sheet.png");
        fontTexture = LoadTexture("font.png");

        //Load map
        if (currentGameLevel == 1){
            map.Load("Level1.txt");
        }
        else if (currentGameLevel == 2){
            map.Load("Level2.txt");
        }
        else if (currentGameLevel == 3){
            map.Load("Level3.txt");
        }
       
        glBindTexture(GL_TEXTURE_2D, spriteSheetTexture);
        for(int y=0; y < map.mapHeight; y++) {
            for(int x=0; x < map.mapWidth; x++) {
                if(map.mapData[y][x] != 0 && map.mapData[y][x] != 12){
                    float u = (float)((map.mapData[y][x]) % sprite_count_x) / (float)(sprite_count_x);
                    float v = (float)((map.mapData[y][x]) / sprite_count_x) / (float)(sprite_count_y);
                    float spriteWidth = 1.0f/(float)sprite_count_x;
                    float spriteHeight = 1.0f/(float)sprite_count_y;
                    vertexData.insert(vertexData.end(), {
                        tileSize * x, -tileSize * y,
                        tileSize * x, (-tileSize * y)-tileSize,
                        (tileSize * x)+tileSize, (-tileSize * y)-tileSize,
                        tileSize * x, -tileSize * y,
                        (tileSize * x)+tileSize, (-tileSize * y)-tileSize,
                        (tileSize * x)+tileSize, -tileSize * y
                    });
                    texCoordData.insert(texCoordData.end(), {
                        u, v,
                        u, v+(spriteHeight),
                        u+spriteWidth, v+(spriteHeight),
                        u, v,
                        u+spriteWidth, v+(spriteHeight),
                        u+spriteWidth, v
                    });
                }
            }
        }
        
        //make certain floor tiles collidable
        for (int ID: palpableBlockIds){
            palpables.push_back(ID);
        }
    }
    void Events(){
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
            else if (event.type == SDL_KEYDOWN){
                if(event.key.keysym.scancode == SDL_SCANCODE_SPACE && JumpOn) {
                    entities[0].velocity.y=1.0;
                    JumpOn = false;
                }
            }
        }
    }
    
    void Update(float elapsedUpdate = elapsed){
        glClear(GL_COLOR_BUFFER_BIT);
        modelMatrix = glm::mat4(1.0f);
        const Uint8 *keys = SDL_GetKeyboardState(NULL);

        //Handle player movement
        if(keys[SDL_SCANCODE_LEFT]) {
            entities[0].velocity.x = -0.25;
        } else if(keys[SDL_SCANCODE_RIGHT]) {
            entities[0].velocity.x = 0.25;
        }
        else{
            entities[0].velocity.x = 0;
        }
        entities[0].velocity.y -= gravity;
        
        
        
        
        if(playerCollideBottom()||playerCollideTop()){
            entities[0].velocity.y = 0;
        }
        
        if(playerCollideLeft()||playerCollideRight()){
            entities[0].velocity.x = 0;
        }
        
        if (enemies.empty()==true){
            cout<<"TESTING"<<endl;
        }
        
        if(enemies.empty() == false){ //Check if all enemies were already killed
            for(Entity& enemy: enemies){ //Collision with enemy
                if(entities[0].collision(enemy)){
                    enemies.pop_back();
                }
            }
        }
        
        if (levelkeys.empty() == false){
            if(entities[0].collision(levelkeys[0])){
                levelkeys.pop_back();
            }
        }

        entities[0].update(elapsedUpdate);
        viewMatrix = glm::mat4(1.0f);
        viewMatrix = glm::translate(viewMatrix, glm::vec3(-entities[0].position.x,-entities[0].position.y,0.0f));
        program.SetViewMatrix(viewMatrix);
    }
    
    void Render(){
        if (!win){
        drawMap();
        
        for(Entity& e: entities){
            e.Draw(program, elapsed);
        }
        if (enemies.empty() == false){
            for(Entity& enemy: enemies){
                enemy.Draw(program, elapsed);
                }
            }
        }
        else {
            DrawText(program, fontTexture, "You Win!", 0.0, -0.90, 0.40, 0.01); //Display YOU WIN to player
        }
    }
    void Clean(){}
};




mainMenu menu;
Game game;
void Setup(){
    game.Setup();
}

void Events(){
    switch(mode){
        case STATE_MAIN_MENU:
            menu.Events();
            break;
        case STATE_GAME_LEVEL:
            game.Events();
            break;
    }
}

void Update(float elapsedUpdate = elapsed){
    switch(mode){
        case STATE_MAIN_MENU:
            menu.Update();
            break;
        case STATE_GAME_LEVEL:
            game.Update(elapsedUpdate);
            break;
    }
}

void Render(){
    switch(mode){
        case STATE_MAIN_MENU:
            menu.Render();
            break;
        case STATE_GAME_LEVEL:
            game.Render();
            break;
    }
}

void Clean(){
    switch(mode){
        case STATE_MAIN_MENU:
            menu.Clean();
            break;
        case STATE_GAME_LEVEL:
            game.Clean();
            break;
    }
}


void renderEntities(){
    for(FlareMapEntity entity : map.entities){
        if(entity.type == "enemy"){
            float u = (float)((102) % sprite_count_x) / (float) sprite_count_x;
            float v = (float)((102) / sprite_count_x) / (float) sprite_count_y;
            float spriteWidth = 1.0f/(float)sprite_count_x;
            float spriteHeight = 1.0f/(float)sprite_count_y;
            SheetSprite enemy = SheetSprite(spriteSheetTexture,u, v,spriteWidth, spriteHeight, tileSize);
            enemies.push_back(Enemy(ent.x*tileSize,ent.y*-tileSize, .07,0,enemy.width,enemy.height, u, v, enemy.textureID, enemy.size, &map, &solids, &entities[0]));
        }
        else if(entity.type == "power"){
            float u = (float)((38) % sprite_count_x) / (float) sprite_count_x;
            float v = (float)((38) / sprite_count_x) / (float) sprite_count_y;
            float spriteWidth = 1.0f/(float)sprite_count_x;
            float spriteHeight = 1.0f/(float)sprite_count_y;
            
            SheetSprite powerSprite = SheetSprite(spriteSheetTexture,u, v,spriteWidth, spriteHeight, tileSize);
            power.push_back(Entity(ent.x*tileSize,ent.y*-tileSize,0,0,powerSprite.width,powerSprite.height,0,0,0,powerSprite.u,powerSprite.v,powerSprite.textureID, powerSprite.size));
        }
        else if(ent.type == "health"){
            float u = (float)((27) % sprite_count_x) / (float) sprite_count_x;
            float v = (float)((27) / sprite_count_x) / (float) sprite_count_y;
            float spriteWidth = 1.0f/(float)sprite_count_x;
            float spriteHeight = 1.0f/(float)sprite_count_y;
            SheetSprite healthSprite = SheetSprite(spriteSheetTexture,u, v,spriteWidth, spriteHeight, tileSize);
            healthUp.push_back(Entity(ent.x*tileSize,ent.y*-tileSize,0,0,healthSprite.width,healthSprite.height,0,0,0,healthSprite.u,healthSprite.v,healthSprite.textureID, healthSprite.size));
        }
    }
}


int main(int argc, char *argv[]){
    Setup();
    float u = (float)((98) % sprite_count_x) / (float) sprite_count_x;
    float v = (float)((98) / sprite_count_x) / (float) sprite_count_y;
    
    float spriteWidth = 1.0f/(float)sprite_count_x;
    float spriteHeight = 1.0f/(float)sprite_count_y;
    
    SheetSprite mySprite = SheetSprite(spriteSheetTexture, u, v,spriteWidth , spriteHeight, tileSize);
    
    u = (float)((80) % sprite_count_x) / (float) sprite_count_x;
    v = (float)((80) / sprite_count_x) / (float) sprite_count_y;
    
    SheetSprite enemy = SheetSprite(spriteSheetTexture, u, v,spriteWidth , spriteHeight, tileSize);
    
    
    u = (float)((86) % sprite_count_x) / (float) sprite_count_x;
    v = (float)((86) / sprite_count_x) / (float) sprite_count_y;
    
    SheetSprite key = SheetSprite(spriteSheetTexture,u, v, spriteWidth , spriteHeight, tileSize);
    
    
    //Instantiate new enemy and player
    Entity newPlayer(0, 0,-1.0,0,mySprite.width,mySprite.height,0,0,0,mySprite.u,mySprite.v,mySprite.textureID, mySprite.size);
    
    Entity newEnemy(1.5,-1.0,-0.1,0.0,enemy.width,enemy.height,0,0,0,enemy.u,enemy.v,enemy.textureID, enemy.size);

    Entity newKey(1.5, -1.0, 0.0, 0.0, tileSize, tileSize, 0, 0, 0, key.u, key.v, key.textureID, key.size);
    
    
    
    //Push enemy and player into their respective vectors
    entities.push_back(newPlayer);
    enemies.push_back(newEnemy);
    levelkeys.push_back(newKey);
    
    
    while (!done) {
        float ticks = (float)SDL_GetTicks()/1000.0f;
        elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        elapsed += accumulator;
        
        if(elapsed < FIXED_TIMESTEP) {
            accumulator = elapsed;
            continue;
        }
        
        while(elapsed >= FIXED_TIMESTEP) {
            Update(FIXED_TIMESTEP);
            elapsed -= FIXED_TIMESTEP;
        }
        
        accumulator = elapsed;
        Events();
        Update();
        Render();
        SDL_GL_SwapWindow(displayWindow);
        
    }
    
    Clean();
    SDL_Quit();
    return 0;
}
