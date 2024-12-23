#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <cstdio>
#include <deque>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <array>
#include <vector>

#define WINDOW_WIDTH 288
#define WINDOW_HEIGHT 512
#define FPS 30
#define MIN_PIPE_LEN 75
#define MIN_DISTANCE 85
#define HITBOX_LENIENCE 1.8

//not uniform, to is inclusive
auto randrange(auto from, auto to) {
    return std::rand() % (to - from + 1) + from;
}

float ground_y;
float speed = 6;

class Obstacle {
    static sf::Texture pipe_up;
    static sf::Texture pipe_down;
    sf::Sprite top;
    sf::Sprite bot;

public:
    static sf::Vector2u texture_size;
    bool scored = false;

    static void init() {
        sf::Image pipe;
        pipe.loadFromFile("assets/sprites/pipe-green.png");
        pipe_up.loadFromImage(pipe);
        pipe.flipVertically();
        pipe_down.loadFromImage(pipe);
        texture_size = pipe.getSize();
    }

    Obstacle() {
        int gap = randrange(75, 110);
        assert(MIN_PIPE_LEN <= (int)ground_y - gap - MIN_PIPE_LEN);
        int bot_len = randrange(MIN_PIPE_LEN, (int)ground_y - gap - MIN_PIPE_LEN);
        assert((int)ground_y >= bot_len + gap);
        int top_len = (int)ground_y - bot_len - gap;
        top.setTexture(pipe_down);
        bot.setTexture(pipe_up);
        assert((int)texture_size.y >= top_len);
        top.setTextureRect({0, (int)texture_size.y - top_len, (int)texture_size.x, top_len});
        bot.setTextureRect({0, 0, (int)texture_size.x, bot_len});
        top.setPosition(WINDOW_WIDTH, 0);
        bot.setPosition(WINDOW_WIDTH, top_len + gap);
    }

    void move(float offsetX) {
        top.move(offsetX, 0);
        bot.move(offsetX, 0);
    }

    void draw(sf::RenderWindow &window) {
        window.draw(top);
        window.draw(bot);
    }

    const sf::Vector2f &getPosition() const {
        return top.getPosition();
    }

    bool collidesWith(sf::FloatRect hitbox) {
        return top.getGlobalBounds().intersects(hitbox)
            || bot.getGlobalBounds().intersects(hitbox);
    }
};

class Bird : public sf::Sprite {
    static sf::Texture bird_midflap;
    static sf::Texture bird_downflap;
    static sf::Texture bird_upflap;
    static sf::SoundBuffer wing_flap_buffer;
    static sf::Sound wing_flap;
    static sf::SoundBuffer woosh_buffer;
    static sf::Sound woosh;

    int animation_timer = 0;
    bool in_animation = false;
    int first_press_timer = 0;

public:
    static void init() {
        bird_midflap.loadFromFile("assets/sprites/bluebird-midflap.png");
        bird_downflap.loadFromFile("assets/sprites/bluebird-downflap.png");
        bird_upflap.loadFromFile("assets/sprites/bluebird-upflap.png");
        wing_flap_buffer.loadFromFile("assets/audio/wing.wav");
        wing_flap = sf::Sound(wing_flap_buffer);
    }

    Bird(sf::Vector2f pos) {
        setTexture(bird_midflap);
        setPosition(pos);
    }

    void flap() {
        wing_flap.play();
        first_press_timer = FPS;
        in_animation = true;
        animation_timer = 0;
        setTexture(bird_downflap);
        setRotation(0);
    }

    void update() {
        if (first_press_timer < FPS / 2) {
            ++first_press_timer;
            return;
        }
        if (!in_animation) {
            if ((int)getRotation() < 40)
                rotate(4);
            move(0, speed * 4);
            return;
        }
        if (animation_timer >= 6) {
            in_animation = false;
            setTexture(bird_midflap);
            return;
        } else if (animation_timer >= 4) {
            setTexture(bird_upflap);
        } else if (animation_timer >= 2) {
            setTexture(bird_midflap);
        }
        move(0, (int)(0.4 * (animation_timer + 5) * (animation_timer - 5)));
        ++animation_timer;
        rotate((int)(-2 * animation_timer * (animation_timer - 3) * (animation_timer - 6)));
    }

    sf::Vector2f center() const {
        sf::Vector2f size = getGlobalBounds().getSize();
        return getPosition() + sf::Vector2f(size.x / 2.0, size.y / 2.0);
    }

    sf::FloatRect hitbox() const {
        sf::FloatRect rect = getGlobalBounds();
        float wsub = rect.width / HITBOX_LENIENCE;
        float hsub = rect.height / HITBOX_LENIENCE;
        rect.width -= wsub;
        rect.height -= hsub;
        if (!in_animation) {
            rect.top += hsub / 2;
            rect.left += wsub / 2;
        } else if (animation_timer > 4) {
            rect.left += wsub / 2;
            rect.top += hsub;
        } else {
            rect.top += hsub / 2;
            rect.left += wsub / 2;
        }
        return rect;
    }
};

class Score {
    static std::array<sf::Texture, 10> numbers;
    static unsigned height;
    static inline const unsigned margin = 4;
    static sf::SoundBuffer sound_buffer;
    static sf::Sound sound;

    std::vector<sf::Sprite> digits;
    const sf::Vector2f pos;
    unsigned width;

public:
    unsigned score = 0;

    static void init() {
        char path[] = "assets/sprites/0.png";
        for (unsigned i = 0; i < 10; ++i) {
            path[15] = i + '0';
            numbers[i].loadFromFile(path);
        }
        height = numbers[0].getSize().y;
        sound_buffer.loadFromFile("assets/audio/point.wav");
        sound = sf::Sound(sound_buffer);
    }

    Score(sf::Vector2f pos) : digits(1), pos(pos) {
        digits[0].setTexture(numbers[0], true);
        recalcPositions();
    }

    void reset() {
        digits.erase(digits.begin() + 1, digits.end());
        digits[0].setTexture(numbers[0], true);
        recalcPositions();
    }

    Score &operator++() {
        sound.play();
        ++score;
        sf::Texture *nine = &numbers[9];
        bool carry = true;
        for (sf::Sprite &digit : digits) {
            if (!carry) break;
            const sf::Texture *texture = digit.getTexture();
            if (texture == nine) {
                digit.setTexture(numbers[0], true);
                carry = true;
            } else {
                digit.setTexture(texture[1], true);
                carry = false;
            }
        }
        if (carry) {
            sf::Sprite &digit = digits.emplace_back();
            digit.setTexture(numbers[1], true);
        }
        recalcPositions();
        return *this;
    }

    operator unsigned() const {
        return score;
    }

    void draw(sf::RenderWindow &window) {
        for (const sf::Sprite &digit : digits) {
            window.draw(digit);
        }
    }

private:
    void recalcPositions() {
        width = 0;
        for (const sf::Sprite &digit : digits) {
            width += digit.getTextureRect().width;
        }
        width += margin * (digits.size() - 1);
        float x = pos.x + width / 2.0;
        float y = pos.y - height / 2.0;
        for (sf::Sprite &digit : digits) {
            unsigned w = digit.getTextureRect().width;
            digit.setPosition(x - w, y);
            x -= w + margin;
        }
    }
};

sf::Texture Obstacle::pipe_up;
sf::Texture Obstacle::pipe_down;
sf::Vector2u Obstacle::texture_size;

sf::Texture Bird::bird_midflap;
sf::Texture Bird::bird_downflap;
sf::Texture Bird::bird_upflap;
sf::SoundBuffer Bird::wing_flap_buffer;
sf::Sound Bird::wing_flap;

std::array<sf::Texture, 10> Score::numbers;
sf::SoundBuffer Score::sound_buffer;
sf::Sound Score::sound;
unsigned Score::height;

int main() {
    Obstacle::init();
    Bird::init();
    Score::init();

    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "SFML");
    window.setFramerateLimit(FPS);
    sf::VideoMode mode = sf::VideoMode::getDesktopMode();
    window.setPosition({
            (int)(mode.width - WINDOW_WIDTH) / 2,
            (int)(mode.height - WINDOW_HEIGHT) / 2});
    
    sf::Texture background_day;
    background_day.loadFromFile("assets/sprites/background-day.png");
    background_day.setRepeated(true);
    sf::Sprite bg;
    bg.setTexture(background_day);
    bg.setTextureRect({0, 0, WINDOW_WIDTH * 2, WINDOW_HEIGHT});

    sf::Texture grass;
    grass.loadFromFile("assets/sprites/base.png");
    sf::Sprite ground;
    ground.setTexture(grass);
    ground_y = (float)WINDOW_HEIGHT - grass.getSize().y;
    ground.setPosition({0, ground_y});

    sf::Vector2f bird_init_pos = {WINDOW_WIDTH / 5.0, WINDOW_HEIGHT / 2.0};
    Bird bird(bird_init_pos);
    Score score({WINDOW_WIDTH / 2.0, WINDOW_HEIGHT / 6.0});
    //while (score != 200) ++score;
    std::deque<Obstacle> obstacles;
    int spawn_timer = 0;
    bool game_over = false;

    sf::SoundBuffer bonk_buffer;
    bonk_buffer.loadFromFile("assets/audio/hit.wav");
    sf::Sound bonk(bonk_buffer);
    sf::Music music;
    music.openFromFile("assets/audio/music.mp3");
    music.setLoop(true);
    music.play();

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            switch (e.type) {
            using enum sf::Event::EventType;
            case Closed: 
                window.close();
                break;
            case KeyPressed:
                bird.flap();
                break;
            default:
                break;
            }
        }

        bird.update();
        if (spawn_timer <= 0) {
            spawn_timer = Obstacle::texture_size.x + randrange(MIN_DISTANCE, MIN_DISTANCE + 50);
            obstacles.emplace_back();
        }
        bg.move(-speed, 0);
        spawn_timer -= speed;
        if (bg.getPosition().x <= -WINDOW_WIDTH) {
            bg.move(WINDOW_WIDTH, 0);
        }
        sf::FloatRect hitbox = bird.hitbox();
        game_over |= ground.getGlobalBounds().intersects(hitbox);
        if (!obstacles.empty()) {
            for (Obstacle &ob : obstacles) {
                ob.move(-speed);
                if (!ob.scored &&
                        ob.getPosition().x + Obstacle::texture_size.x < bird.center().x)
                {
                    ob.scored = true;
                    ++score;
                }
                game_over |= ob.collidesWith(hitbox);
            }
            Obstacle &front = obstacles.front();
            if (front.getPosition().x + Obstacle::texture_size.x <= 0) {
                obstacles.pop_front();
            }
        }

        window.clear();
        window.draw(bg);
        window.draw(ground);
        for (Obstacle &ob : obstacles) {
            ob.draw(window);
        }
        score.draw(window);
        window.draw(bird);
        //sf::RectangleShape rect(bird.hitbox().getSize());
        //rect.setFillColor(sf::Color::Transparent);
        //rect.setOutlineColor(sf::Color::Black);
        //rect.setOutlineThickness(2.0);
        //rect.setPosition(bird.hitbox().getPosition());
        //window.draw(rect);
        window.display();
        
        if (game_over) {
            bonk.play();
            music.stop();
        }
            
        while (game_over) {
            while (window.pollEvent(e)) {
                switch (e.type) {
                using enum sf::Event::EventType;
                case Closed: 
                    window.close();
                    break;
                case MouseButtonPressed:
                    obstacles.clear();
                    spawn_timer = 0;
                    game_over = false;
                    bird = Bird(bird_init_pos);
                    score.reset();
                    music.play();
                    break;
                default:
                    break;
                }
            }
        }
    }
    return 0;
}
