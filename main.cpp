/**
* Author: Selina Gong
* Assignment: Lunar Lander
* Date due: 2024-10-27, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/


#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 1.0f/60.0f
#define PLATFORM_COUNT 10


#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include <cstdlib>
#include "Entity.h"

// ––––– STRUCTS AND ENUMS ––––– //
struct GameState
{
    Entity* rocket;
    Entity* platforms;
};


// ––––– CONSTANTS ––––– //
const int WINDOW_WIDTH  = 640*1.5,
          WINDOW_HEIGHT = 480*1.5;

const float BG_RED     = 0.08235f,
            BG_GREEN   = 0.1098f,
            BG_BLUE    = 0.2588f,
            BG_OPACITY = 1.0f;

const int VIEWPORT_X = 0,
          VIEWPORT_Y = 0,
          VIEWPORT_WIDTH  = WINDOW_WIDTH,
          VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
           F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;
const char ROCKET_FILEPATH[] = "assets/rocket.png";
const char PLATFORM_FILEPATH[]    = "assets/platform_mission_fail.png";
const char PLATFORM_LANDING_FILEPATH[] = "assets/platform_landing.png";
constexpr char FONTSHEET_FILEPATH[] = "assets/font1.png";
constexpr int FONTBANK_SIZE = 16;
std::string display_message = "";
std::string display_message_restart = "";
ShaderProgram g_shader_program;
GLuint g_font_texture_id;
GLuint platform_texture_id;
GLuint platform_landing_texture_id;

const int NUMBER_OF_TEXTURES = 1;
const GLint LEVEL_OF_DETAIL  = 0;
const GLint TEXTURE_BORDER   = 0;

constexpr float ACC_OF_GRAVITY = -0.3f; // gravity


// ––––– GLOBAL VARIABLES ––––– //
GameState g_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;

int LANDING_PLATFORM;
constexpr glm::vec3 ROCKET_INIT_SCALE = glm::vec3(1.5f, 0.75f, 0.0f),
                    ROCKET_INIT_POS = glm::vec3(0.0f, 3.0f, 0.0f);


// ––––– GENERAL FUNCTIONS ––––– //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);
    
    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }
    
    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    stbi_image_free(image);
    
    return textureID;
}

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text,
    float font_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for
    // each character. Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their
        //    position relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (font_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->set_model_matrix(model_matrix);
    glUseProgram(program->get_program_id());

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0,
        vertices.data());
    glEnableVertexAttribArray(program->get_position_attribute());
    glVertexAttribPointer(program->get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0,
        texture_coordinates.data());
    glEnableVertexAttribArray(program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(program->get_position_attribute());
    glDisableVertexAttribArray(program->get_tex_coordinate_attribute());
}

void initialise()
{
    srand(time(0));
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow("That is not Nyan Cat, I tried to draw Nyan Cat from memory, and it went badly. That is not Nyan Cat.",
                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      WINDOW_WIDTH, WINDOW_HEIGHT,
                                      SDL_WINDOW_OPENGL);
    
    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);
    
#ifdef _WINDOWS
    glewInit();
#endif
    

    // ––––– VIDEO ––––– //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
    
    g_program.load(V_SHADER_PATH, F_SHADER_PATH);
    
    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);
    
    g_program.set_projection_matrix(g_projection_matrix);
    g_program.set_view_matrix(g_view_matrix);
    
    glUseProgram(g_program.get_program_id());
    
    glClearColor(BG_RED, BG_GREEN, BG_BLUE, BG_OPACITY);
    
    // ––––– PLATFORMS ––––– //
    platform_texture_id = load_texture(PLATFORM_FILEPATH);
    platform_landing_texture_id = load_texture(PLATFORM_LANDING_FILEPATH);
    
    g_state.platforms = new Entity[PLATFORM_COUNT];
    
    // Set the type of every platform entity to PLATFORM
    for (int i = 0; i < PLATFORM_COUNT; i++)
    {
        g_state.platforms[i].set_texture_id(platform_texture_id);
        g_state.platforms[i].set_position(glm::vec3(i - 4.5f, -3.4f, 0.0f));
        g_state.platforms[i].set_width(1.0f);
        g_state.platforms[i].set_height(1.0f);
        g_state.platforms[i].set_entity_type(PLATFORM);
        g_state.platforms[i].update(0.0f, NULL, NULL, 0);
    }
    
    LANDING_PLATFORM = rand() % PLATFORM_COUNT;
    g_state.platforms[LANDING_PLATFORM].set_entity_type(LANDING);
    g_state.platforms[LANDING_PLATFORM].set_texture_id(platform_landing_texture_id);


    // ––––– ROCKET––––– //
    // Existing
    GLuint rocket_texture_id = load_texture(ROCKET_FILEPATH);
    glm::vec3 acceleration = glm::vec3(0.0f,-4.905f, 0.0f);

    g_state.rocket = new Entity(
        rocket_texture_id,         // texture id
        2.0f,                      // speed
        1.0f,                      // width
        1.0f,                      // height
        ROCKET
    );
    
    g_state.rocket->set_scale(ROCKET_INIT_SCALE);
    g_state.rocket->set_position(ROCKET_INIT_POS);
    g_state.rocket->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.05, 0.0f));
    // very bad idea, but if it works, it works ToT (this is literally only for the scale code)
    g_state.rocket->update(0, NULL, g_state.platforms, PLATFORM_COUNT);
    g_state.rocket->set_can_move(false);

    
    // ----- TEXT ------//
    g_font_texture_id = load_texture(FONTSHEET_FILEPATH);
    g_state.rocket->set_display_message("Press [t] to start");
    g_state.rocket->set_display_message_reset("");

    // ––––– GENERAL ––––– //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_state.rocket->set_movement(glm::vec3(0.0f));
    
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
            case SDL_QUIT:
            case SDL_WINDOWEVENT_CLOSE:
                g_game_is_running = false;
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_q:
                        // Quit the game with a keystroke
                        g_game_is_running = false;
                        break;

                    case SDLK_t:
                        g_state.rocket->set_can_move(true);
                        g_state.rocket->set_display_message("");
                        g_state.rocket->set_display_message_reset("");
                        break;

                    case SDLK_r:
                        g_state.rocket->set_position(ROCKET_INIT_POS);
                        g_state.rocket->update(0, NULL, g_state.platforms, PLATFORM_COUNT);
                        g_state.rocket->set_display_message("Press [t] to start");
                        g_state.rocket->set_display_message_reset("");

                        g_state.platforms[LANDING_PLATFORM].set_texture_id(platform_texture_id);
                        g_state.platforms[LANDING_PLATFORM].set_entity_type(PLATFORM);
                        LANDING_PLATFORM = rand() % PLATFORM_COUNT;
                        g_state.platforms[LANDING_PLATFORM].set_entity_type(LANDING);
                        g_state.platforms[LANDING_PLATFORM].set_texture_id(platform_landing_texture_id);
                        break;

                    default:
                        break;
                }
                
            default:
                break;
        }
    }
    
    const Uint8 *key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_LEFT])
    {
        //g_state.rocket->move_left();
        g_state.rocket->accelerate_left();
    }
    else if (key_state[SDL_SCANCODE_RIGHT])
    {
        //g_state.rocket->move_right();
        g_state.rocket->accelerate_right();

    }
    
    if (glm::length(g_state.rocket->get_movement()) > 1.0f)
    {
        g_state.rocket->normalise_movement();
    }
}

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;
    
    delta_time += g_time_accumulator;
    
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }
    

    while (delta_time >= FIXED_TIMESTEP)
    {
        if (g_state.rocket->get_can_move())
        g_state.rocket->update(FIXED_TIMESTEP, NULL, g_state.platforms, PLATFORM_COUNT);
        delta_time -= FIXED_TIMESTEP;
    }

    
    
    g_time_accumulator = delta_time;
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);
    
    draw_text(&g_program, g_font_texture_id, g_state.rocket->get_display_message(), 0.3f, 0.05f,
        glm::vec3(-3.5f, 2.0f, 0.0f));
    draw_text(&g_program, g_font_texture_id, g_state.rocket->get_display_message_reset(), 0.3f, 0.05f,
        glm::vec3(-3.5f, 1.0f, 0.0f));

    g_state.rocket->render(&g_program);
    
    for (int i = 0; i < PLATFORM_COUNT; i++) {
        //if (g_state.platforms[i].get_entity_type() != LANDING)
            g_state.platforms[i].render(&g_program);
    }
    
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();
    
    delete [] g_state.platforms;
    delete g_state.rocket;
}

// ––––– GAME LOOP ––––– //
int main(int argc, char* argv[])
{
    initialise();
    
    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }
    
    shutdown();
    return 0;
}