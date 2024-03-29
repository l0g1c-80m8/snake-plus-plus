#include <thread>
#include <future>
#include "game.h"
#include "SDL.h"

Game::Game(std::size_t grid_width, std::size_t grid_height)
    :
      engine(dev()),
      random_w(0, static_cast<int>(grid_width - 1)),
      random_h(0, static_cast<int>(grid_height - 1)) {
  config = Config::Instance();
  snake = Snake::Instance(static_cast<int>(grid_width), static_cast<int>(grid_height));
  obstacles = GenerateGridPoints(config -> getObstacles());
  slowdowns = GenerateGridPoints(config -> getSlowdowns());
  speedups = GenerateGridPoints(config -> getSpeedups());
  food_points = GenerateGridPoints(config -> getFoodPoints());
}

void Game::Run(Controller const &controller, Renderer &renderer,
               std::size_t target_frame_duration) {
  Uint32 title_timestamp = SDL_GetTicks();
  Uint32 frame_start;
  Uint32 frame_end;
  Uint32 frame_duration;
  int frame_count = 0;
  bool running = true;

  while (running) {
    frame_start = SDL_GetTicks();

    // Input, Update, Render - the main game loop.
    Controller::HandleInput(running, snake);
    Update();
    renderer.Render(snake, obstacles, slowdowns, speedups, food_points);

    frame_end = SDL_GetTicks();

    // Keep track of how long each loop through the input/update/render cycle
    // takes.
    frame_count++;
    frame_duration = frame_end - frame_start;

    // After every second, update the window title.
    if (frame_end - title_timestamp >= 1000) {
      renderer.UpdateWindowTitle(score, frame_count);
      frame_count = 0;
      title_timestamp = frame_end;
    }

    // If the time for this frame is too small (i.e. frame_duration is
    // smaller than the target ms_per_frame), delay the loop to
    // achieve the correct frame rate.
    if (frame_duration < target_frame_duration) {
      SDL_Delay(target_frame_duration - frame_duration);
    }
  }
}

void Game::PlaceGridItem(SDL_Point gridPoint, Game::GridItemType itemType) {
  switch (itemType) {
    case Game::GridItemType::giObstacle:
      obstacles.erase(gridPoint);
      obstacles.insert(GenerateGridPoint());
      break;
    case Game::GridItemType::giSlowdown:
      slowdowns.erase(gridPoint);
      slowdowns.insert(GenerateGridPoint());
      break;
    case Game::GridItemType::giSpeedup:
      speedups.erase(gridPoint);
      speedups.insert(GenerateGridPoint());
      break;
    case Game::GridItemType::giFood:
      food_points.erase(gridPoint);
      food_points.insert(GenerateGridPoint());
      break;
  }
}

void Game::Update() {
  if (!snake -> alive) return;

  snake -> Update();

  int new_x = static_cast<int>(snake -> head_x);
  int new_y = static_cast<int>(snake -> head_y);

  // Check if there's any grid item at current head pose
  if (food_points.find(SDL_Point{new_x, new_y}) != food_points.end()) {
    score++;
    PlaceGridItem(SDL_Point{new_x, new_y}, Game::GridItemType::giFood);
    // Grow snake and increase speed.
    snake -> GrowBody();
    snake -> speed += 0.02;
  } else if (obstacles.find(SDL_Point{new_x, new_y}) != obstacles.end()) {
    snake -> alive = false;
  } else if (slowdowns.find(SDL_Point{new_x, new_y}) != slowdowns.end()) {
    slow_downs++;
    PlaceGridItem(SDL_Point{new_x, new_y}, Game::GridItemType::giSlowdown);
    snake -> speed -= 0.02;
  } else if (speedups.find(SDL_Point{new_x, new_y}) != speedups.end()) {
    speed_ups++;
    PlaceGridItem(SDL_Point{new_x, new_y}, Game::GridItemType::giSpeedup);
    snake -> speed += 0.02;
  }
}

int Game::GetScore() const { return score; }
int Game::GetSlowdowns() const { return slow_downs; }
int Game::GetSpeedups() const { return speed_ups; }
int Game::GetSize() const { return snake -> size; }

std::set<SDL_Point> Game::GenerateGridPoints(int num) {
  std::set<SDL_Point> pts{};
  while (pts.size() < num) {
    std::vector<std::future<SDL_Point>> future_points;
    std::vector<std::thread> thread_pool;
    for (int i = 0; i < num - pts.size(); i++) {
      std::promise<SDL_Point> promise;
      future_points.push_back(promise.get_future());
      thread_pool.emplace_back([this](std::promise<SDL_Point> promise) {
          promise.set_value(GenerateGridPoint());
      }, std::move(promise));
    }
    for (auto & worker_thread : thread_pool) worker_thread.join();
    for (auto & future_point : future_points) pts.insert(future_point.get());
  }
  return pts;
}

SDL_Point Game::GenerateGridPoint() {
  while (true) {
    int x, y;
    x = random_w(engine);
    y = random_h(engine);
    // Check that the location is not occupied by a grid item before placing a new item.
    if (!snake -> SnakeCell(x, y)
    && obstacles.find(SDL_Point{x, y}) == obstacles.end()
    && slowdowns.find(SDL_Point{x, y}) == slowdowns.end()
    && speedups.find(SDL_Point{x, y}) == speedups.end()
    && food_points.find(SDL_Point{x, y}) == food_points.end()
    )
      return SDL_Point {x, y};
  }
}

bool operator<(const SDL_Point &pt1, const SDL_Point &pt2)
{
  if (pt1.x != pt2.x) return pt1.x < pt2.x;
  if (pt1.y != pt2.y) return pt1.y < pt2.y;
  return false;
}