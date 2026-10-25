#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>
#include <string>

#include "window.hpp"

namespace Dream
{

    //def structs

    struct Player
    {
        double posX = 2.5;
        double posY = 2.5;
        double dirX = 1.0;
        double dirY = 0.0;
        double planeX = 0.0;
        double planeY = 0.66;

        int health = 100;
        int maxHealth = 100;
    };

    enum class EnemyState
    {
        Guard,
        Chase
    };

    enum class EnemyAIType
    {
        Guard,
        Autonomous
    };

    struct SpriteMaterial
    {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    struct EnemySpriteSet
    {
        std::array<SpriteMaterial, 8> directions;
    };

    struct Enemy
    {
        double posX = 0.0;
        double posY = 0.0;
        double angle = 0.0;

        int health = 100;
        int damage = 10;

        EnemyState state = EnemyState::Guard;

        double detectionRange = 8.0;
        double attackRange = 4.0;
        double moveSpeed = 1.2;
        double turnSpeed = 2.5;

        double attackCooldown = 0.0;
        double attackInterval = 1.5;

        EnemySpriteSet* sprites = nullptr;

        bool active = true;

        double moveTimer = 0.0;
        double turnTimer = 0.0;

        double targetAngle = 0.0;

        double autonomousTurnSpeed = 0.8;
        double autonomousMoveSpeed = 0.7;

        bool moving = false;

        EnemyAIType aiType = EnemyAIType::Guard;

        bool patrolRotation = true;
    };

    struct Projectile
    {
        double posX = 0.0;
        double posY = 0.0;

        double velocityX = 0.0;
        double velocityY = 0.0;

        double speed = 4.0;
        double radius = 0.08;

        int damage = 10;

        bool active = true;
    };

    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;

    std::vector<double> wallDepthBuffer;

    


    struct RayHit
    {
        bool hit = false;

        double distance = 0.0;

        int mapX = 0;
        int mapY = 0;

        int side = 0; 

        int wallType = 0;
    };

    struct WallMaterial
    {
        SDL_Texture* texture = nullptr;

        int width = 0;
        int height = 0;
    };

    WallMaterial wallMaterials[5];

    bool LoadSpriteMaterial(SDL_Renderer* renderer, SpriteMaterial& material, const char* path)
    {
        SDL_Surface* surface = IMG_Load(path);

        if (!surface)
        {
            SDL_Log(
                "Error cargando sprite %s: %s",
                path,
                IMG_GetError()
            );
            return false;
        }

        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(renderer, surface);

        if (!texture)
        {
            SDL_Log("Error creando sprite: %s", SDL_GetError());
            SDL_FreeSurface(surface);
            return false;
        }

        const int width = surface->w;
        const int height = surface->h;

        SDL_FreeSurface(surface);

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

        if (material.texture)
            SDL_DestroyTexture(material.texture);

        material.texture = texture;
        material.width = width;
        material.height = height;

        return true;
    }


    Player player;

    SDL_Texture* skyTexture = nullptr;
    int skyWidth = 0;
    int skyHeight = 0;

    SDL_Texture* floorTexture = nullptr;
    int floorWidth = 0;
    int floorHeight = 0;

    SDL_Texture* floorRenderTexture = nullptr;

    Uint32* floorPixels = nullptr;

    int floorRenderWidth = 0;
    int floorRenderHeight = 0;
    int floorPitch = 0;


    Uint32* floorSourcePixels = nullptr;
    int floorSourcePitch = 0;

    bool LoadEnemySprites(SDL_Renderer* renderer, EnemySpriteSet& sprites, const char* directory)
    {
        const char* filenames[8] =
        {
            "sprite1.png",
            "sprite2.png",
            "sprite3.png",
            "sprite4.png",
            "sprite5.png",
            "sprite6.png",
            "sprite7.png",
            "sprite8.png"
        };

        for (int i = 0; i < 8; ++i)
        {
            const std::string path =
                std::string(directory) + "/" + filenames[i];

            if (!LoadSpriteMaterial(
                    renderer, sprites.directions[i], path.c_str()))
            {
                return false;
            }
        }

        return true;
    }

    bool CreateFloorRenderTexture(SDL_Renderer* renderer)
    {
        floorRenderWidth = Dream::ScreenWidth;
        floorRenderHeight = Dream::ScreenHeight;

        floorRenderTexture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            floorRenderWidth,
            floorRenderHeight
        );

        if (!floorRenderTexture)
        {
            SDL_Log("Error creando textura del suelo: %s",
                    SDL_GetError());
            return false;
        }

        SDL_SetTextureScaleMode(
            floorRenderTexture,
            SDL_ScaleModeNearest
        );

        return true;
    }

    bool LoadFloorTexture(SDL_Renderer* renderer, const char* path)
    {
        SDL_Surface* loaded = IMG_Load(path);

        if (!loaded)
        {
            SDL_Log("Error cargando suelo: %s", IMG_GetError());
            return false;
        }

        SDL_Surface* surface = SDL_ConvertSurfaceFormat(
            loaded,
            SDL_PIXELFORMAT_ARGB8888,
            0
        );

        SDL_FreeSurface(loaded);

        if (!surface)
        {
            SDL_Log("Error convirtiendo suelo: %s", SDL_GetError());
            return false;
        }

        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(renderer, surface);

        if (!texture)
        {
            SDL_Log("Error creando textura: %s", SDL_GetError());
            SDL_FreeSurface(surface);
            return false;
        }

        const int newWidth = surface->w;
        const int newHeight = surface->h;
        const int newPitch = newWidth * sizeof(Uint32);

        Uint32* newPixels = new Uint32[
            static_cast<size_t>(newWidth) * newHeight
        ];

        for (int y = 0; y < newHeight; ++y)
        {
            const auto* src = reinterpret_cast<const Uint32*>(
                static_cast<const Uint8*>(surface->pixels)
                + y * surface->pitch
            );

            std::copy(
                src,
                src + newWidth,
                newPixels + static_cast<size_t>(y) * newWidth
            );
        }

        SDL_FreeSurface(surface);

        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

        if (floorTexture)
            SDL_DestroyTexture(floorTexture);

        delete[] floorSourcePixels;

        floorTexture = texture;
        floorWidth = newWidth;
        floorHeight = newHeight;
        floorSourcePitch = newPitch;
        floorSourcePixels = newPixels;

        return true;
    }


    void RenderFloor(SDL_Renderer* renderer)
    {
        if (!floorSourcePixels ||
            !floorRenderTexture ||
            floorWidth <= 0 ||
            floorHeight <= 0)
        {
            return;
        }

        void* pixels = nullptr;
        int pitch = 0;

        if (SDL_LockTexture(
                floorRenderTexture,
                nullptr,
                &pixels,
                &pitch) != 0)
        {
            SDL_Log("Error bloqueando textura: %s", SDL_GetError());
            return;
        }

        const int screenWidth = Dream::ScreenWidth;
        const int screenHeight = Dream::ScreenHeight;
        const int horizon = screenHeight / 2;

        auto* output = static_cast<Uint32*>(pixels);
        const int stride = pitch / sizeof(Uint32);

        // Exactamente los mismos vectores que el RenderFloor original.
        const double rayDirX0 = player.dirX - player.planeX;
        const double rayDirY0 = player.dirY - player.planeY;

        const double rayDirX1 = player.dirX + player.planeX;
        const double rayDirY1 = player.dirY + player.planeY;

        const double posZ = 0.5 * screenHeight;

        // Limpiar la mitad superior para evitar basura en la textura.
        for (int y = 0; y < horizon; ++y)
        {
            std::fill(
                output + static_cast<size_t>(y) * stride,
                output + static_cast<size_t>(y) * stride + screenWidth,
                0xFF000000u
            );
        }

        // Misma proyección por filas que tu implementación original.
        for (int y = horizon; y < screenHeight; ++y)
        {
            const int p = y - horizon;

            if (p == 0)
                continue;

            const double rowDistance = posZ / p;

            const double floorStepX =
                rowDistance * (rayDirX1 - rayDirX0) / screenWidth;

            const double floorStepY =
                rowDistance * (rayDirY1 - rayDirY0) / screenWidth;

            double floorX =
                player.posX + rowDistance * rayDirX0;

            double floorY =
                player.posY + rowDistance * rayDirY0;

            Uint32* row =
                output + static_cast<size_t>(y) * stride;

            for (int x = 0; x < screenWidth; ++x)
            {
                const double cellX = floorX - std::floor(floorX);
                const double cellY = floorY - std::floor(floorY);

                const int rawTexX = static_cast<int>(cellX * floorWidth);
                const int texX = rawTexX < 0
                    ? 0
                    : (rawTexX >= floorWidth ? floorWidth - 1 : rawTexX);

                const int rawTexY = static_cast<int>(cellY * floorHeight);
                const int texY = rawTexY < 0
                    ? 0
                    : (rawTexY >= floorHeight ? floorHeight - 1 : rawTexY);

                row[x] = floorSourcePixels[
                    static_cast<size_t>(texY) * floorWidth + texX
                ];

                floorX += floorStepX;
                floorY += floorStepY;
            }
        }

        SDL_UnlockTexture(floorRenderTexture);

        SDL_Rect source{
        0,
        Dream::ScreenHeight / 2,
        Dream::ScreenWidth,
        Dream::ScreenHeight / 2
    };

    SDL_Rect destination = source;

    SDL_RenderCopy(
        renderer,
        floorRenderTexture,
        &source,
        &destination
    );
    }

    bool LoadSky(SDL_Renderer* renderer, const char* path)
    {
        SDL_Surface* surface = IMG_Load(path);

        if (!surface)
        {
            SDL_Log("Error cargando sky: %s", IMG_GetError());
            return false;
        }

        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(renderer, surface);

        if (!texture)
        {
            SDL_Log("Error creando sky texture: %s", SDL_GetError());
            SDL_FreeSurface(surface);
            return false;
        }

        skyWidth = surface->w;
        skyHeight = surface->h;
        SDL_FreeSurface(surface);

        // Liberar una textura anterior, si existe.
        if (skyTexture)
            SDL_DestroyTexture(skyTexture);

        skyTexture = texture;

        return true;
    }
    bool LoadWallMaterial(SDL_Renderer* renderer, int materialId, const char* path)
    {
        if (materialId <= 0 || materialId >= 5)
        {
            SDL_Log(
                "ID de material invalido: %d",
                materialId
            );
            return false;
        }

        SDL_Surface* surface = IMG_Load(path);

        if (!surface)
        {
            SDL_Log(
                "Error cargando material %d: %s",
                materialId,
                IMG_GetError()
            );
            return false;
        }

        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(renderer, surface);

        if (!texture)
        {
            SDL_Log(
                "Error creando material %d: %s",
                materialId,
                SDL_GetError()
            );

            SDL_FreeSurface(surface);
            return false;
        }

        const int width = surface->w;
        const int height = surface->h;

        SDL_FreeSurface(surface);

        SDL_SetTextureScaleMode(
            texture,
            SDL_ScaleModeNearest
        );

        SDL_SetTextureColorMod(
            texture, 255, 255, 255
        );

        WallMaterial& material = wallMaterials[materialId];

        // Permitir recargar un material sin perder su textura anterior.
        if (material.texture)
        {
            SDL_DestroyTexture(material.texture);
        }

        material.texture = texture;
        material.width = width;
        material.height = height;

        return true;
    }
    void RenderSky(SDL_Renderer* renderer)
    {
        if (!skyTexture || skyWidth <= 0 || skyHeight <= 0)
            return;

        const int screenWidth = Dream::ScreenWidth;
        const int skyScreenHeight = Dream::ScreenHeight / 2;

        // Zona central de la imagen que utilizaremos como cielo.
        const int sourceWidth = skyWidth / 2;
        const int sourceHeight = skyHeight;
        const int maxSourceX = skyWidth - sourceWidth;

        // Ángulo actual de la cámara.
        const double angle = std::atan2(
            player.dirY,
            player.dirX
        );

        // Normalizar el ángulo entre -PI y PI.
        const double normalizedAngle = std::atan2(
            std::sin(angle),
            std::cos(angle)
        );

        // Limitar el giro para no mostrar los extremos.
        constexpr double maxAngle = M_PI ;

        const double clampedAngle = std::max(
            -maxAngle,
            std::min(normalizedAngle, maxAngle)
        );

        // Convertir el ángulo en desplazamiento horizontal.
        const double rotationFactor =
            clampedAngle / (2.0 * maxAngle);

        const int sourceX = static_cast<int>(
            (rotationFactor + 0.5) * maxSourceX
        );

        SDL_Rect source{
            sourceX,
            0,
            sourceWidth,
            sourceHeight
        };

        SDL_Rect destination{
            0,
            0,
            screenWidth,
            skyScreenHeight
        };

        SDL_RenderCopy(
            renderer,
            skyTexture,
            &source,
            &destination
        );
    }



    
    void MovementPlayer(double deltaTime)
    {
        const Uint8* keyState = SDL_GetKeyboardState(nullptr);

        const double moveSpeed = 3.0;
        const double rotSpeed = 2.5;

        const double moveStep = moveSpeed * deltaTime;
        const double rotStep = rotSpeed * deltaTime;

        auto IsWalkable = [](double x, double y) -> bool
        {
            // Comprobar antes de convertir a int.
            if (x < 0.0 || y < 0.0 ||
                x >= Dream::MapWidth ||
                y >= Dream::MapHeight)
            {
                return false;
            }

            const int mapX = static_cast<int>(x);
            const int mapY = static_cast<int>(y);

            return Dream::Map[mapY][mapX] == 0;
        };

        // Movimiento hacia delante y atrás.
        double moveDir = 0.0;

        if (keyState[SDL_SCANCODE_W] ||
            keyState[SDL_SCANCODE_UP])
        {
            moveDir += 1.0;
        }

        if (keyState[SDL_SCANCODE_S] ||
            keyState[SDL_SCANCODE_DOWN])
        {
            moveDir -= 1.0;
        }

        

        if (moveDir != 0.0)
        {
            const double nextX =
                player.posX + player.dirX * moveStep * moveDir;

            const double nextY =
                player.posY + player.dirY * moveStep * moveDir;

            // Colisión independiente por eje.
            if (IsWalkable(nextX, player.posY))
            {
                player.posX = nextX;
            }

            if (IsWalkable(player.posX, nextY))
            {
                player.posY = nextY;
            }
        }

                // Movimiento lateral (strafe)
        double strafeDir = 0.0;

        if (keyState[SDL_SCANCODE_A])
        {
            strafeDir -= 1.0;
        }

        if (keyState[SDL_SCANCODE_D])
        {
            strafeDir += 1.0;
        }

        if (strafeDir != 0.0)
        {
            // Vector perpendicular a la dirección de la cámara.
            const double strafeX = -player.dirY;
            const double strafeY = player.dirX;

            const double nextX =
                player.posX + strafeX * moveStep * strafeDir;

            const double nextY =
                player.posY + strafeY * moveStep * strafeDir;

            // Colisiones independientes por eje.
            if (IsWalkable(nextX, player.posY))
            {
                player.posX = nextX;
            }

            if (IsWalkable(player.posX, nextY))
            {
                player.posY = nextY;
            }
        }

        // Rotación de cámara.
        double rotation = 0.0;

        if (keyState[SDL_SCANCODE_E] ||
            keyState[SDL_SCANCODE_RIGHT])
        {
            rotation += rotStep;
        }

        if (keyState[SDL_SCANCODE_Q] ||
            keyState[SDL_SCANCODE_LEFT])
        {
            rotation -= rotStep;
        }

        if (rotation != 0.0)
        {
            const double cosRot = std::cos(rotation);
            const double sinRot = std::sin(rotation);

            // Dirección de la cámara.
            const double oldDirX = player.dirX;

            player.dirX =
                oldDirX * cosRot - player.dirY * sinRot;

            player.dirY =
                oldDirX * sinRot + player.dirY * cosRot;

            // Plano de cámara.
            const double oldPlaneX = player.planeX;

            player.planeX =
                oldPlaneX * cosRot - player.planeY * sinRot;

            player.planeY =
                oldPlaneX * sinRot + player.planeY * cosRot;
        }
    }


    RayHit CastRay(double rayDirX, double rayDirY)
    {
        RayHit result{};

        // Celda donde comienza el rayo.
        int mapX = static_cast<int>(player.posX);
        int mapY = static_cast<int>(player.posY);

        const double deltaDistX =
            (rayDirX == 0.0)
            ? 1e30
            : std::abs(1.0 / rayDirX);

        const double deltaDistY =
            (rayDirY == 0.0)
            ? 1e30
            : std::abs(1.0 / rayDirY);

        int stepX;
        int stepY;

        double sideDistX;
        double sideDistY;

        if (rayDirX < 0.0)
        {
            stepX = -1;
            sideDistX =
                (player.posX - mapX) * deltaDistX;
        }
        else
        {
            stepX = 1;
            sideDistX =
                (mapX + 1.0 - player.posX) * deltaDistX;
        }

        if (rayDirY < 0.0)
        {
            stepY = -1;
            sideDistY =
                (player.posY - mapY) * deltaDistY;
        }
        else
        {
            stepY = 1;
            sideDistY =
                (mapY + 1.0 - player.posY) * deltaDistY;
        }

        int side = 0;

        while (true)
        {
            if (sideDistX < sideDistY)
            {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            }
            else
            {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (mapX < 0 || mapX >= Dream::MapWidth ||
                mapY < 0 || mapY >= Dream::MapHeight)
            {
                return result;
            }

            if (Dream::Map[mapY][mapX] != 0)
            {
                result.hit = true;
                result.mapX = mapX;
                result.mapY = mapY;
                result.side = side;
                result.wallType = Dream::Map[mapY][mapX];

                if (side == 0)
                {
                    result.distance =
                        (mapX - player.posX
                        + (1 - stepX) / 2.0) / rayDirX;
                }
                else
                {
                    result.distance =
                        (mapY - player.posY
                        + (1 - stepY) / 2.0) / rayDirY;
                }

                return result;
            }
        }
    }

    bool IsWalkable(double x, double y)
    {
        if (x < 0.0 || y < 0.0 ||
            x >= Dream::MapWidth ||
            y >= Dream::MapHeight)
        {
            return false;
        }

        return Dream::Map[
            static_cast<int>(y)
        ][
            static_cast<int>(x)
        ] == 0;
    }

    bool HasLineOfSight(double x1, double y1, double x2, double y2)
    {
        const double dx = x2 - x1;
        const double dy = y2 - y1;

        const double distance = std::sqrt(dx * dx + dy * dy);

        if (distance < 0.001)
            return true;

        const double stepSize = 0.05;
        const int steps = static_cast<int>(
            std::ceil(distance / stepSize)
        );

        for (int i = 1; i < steps; ++i)
        {
            const double t = static_cast<double>(i) / steps;

            const double x = x1 + dx * t;
            const double y = y1 + dy * t;

            if (!IsWalkable(x, y))
                return false;
        }

        return true;
    }
    double NormalizeAngle(double angle)
{
    constexpr double PI = 3.14159265358979323846;

    while (angle > PI)
        angle -= 2.0 * PI;

    while (angle < -PI)
        angle += 2.0 * PI;

    return angle;
}

void DamagePlayer(int damage)
{
    if (damage <= 0 || player.health <= 0)
        return;

    player.health = std::max(0, player.health - damage);

    if (player.health == 0)
        SDL_Log("Game Over");
}

void SpawnProjectile(Enemy& enemy)
{
    const double dx = player.posX - enemy.posX;
    const double dy = player.posY - enemy.posY;
    const double length = std::sqrt(dx * dx + dy * dy);

    if (length < 0.001)
        return;

    Projectile projectile;

    projectile.posX = enemy.posX;
    projectile.posY = enemy.posY;

    projectile.velocityX = dx / length;
    projectile.velocityY = dy / length;

    projectile.speed = 4.0;
    projectile.damage = enemy.damage;
    projectile.radius = 0.08;

    projectiles.push_back(projectile);
}

    void UpdateEnemies(double deltaTime)
    {
        
        for (Enemy& enemy : enemies)
        {
            if (!enemy.active)
                continue;

            enemy.attackCooldown = std::max(
                0.0, enemy.attackCooldown - deltaTime
            );
            if (enemy.aiType == EnemyAIType::Autonomous)
            {
                const double dx = player.posX - enemy.posX;
                const double dy = player.posY - enemy.posY;

                const double distance = std::sqrt(dx * dx + dy * dy);

                const bool canSeePlayer =
                    distance <= enemy.detectionRange &&
                    HasLineOfSight(
                        enemy.posX, enemy.posY,
                        player.posX, player.posY
                    );

                if (!canSeePlayer)
                {
                    // Girar continuamente sobre sí mismo.
                    enemy.angle += enemy.autonomousTurnSpeed * deltaTime;

                    enemy.angle = NormalizeAngle(enemy.angle);

                    continue;
                }

                // Si te detecta, no ejecutar la IA de guardia.
                // Por ahora permanece en su orientación actual.
                continue;
            }

            const double dx = player.posX - enemy.posX;
            const double dy = player.posY - enemy.posY;

            const double distance = std::sqrt(dx * dx + dy * dy);

            const bool canSeePlayer =
                distance <= enemy.detectionRange &&
                HasLineOfSight(
                    enemy.posX, enemy.posY,
                    player.posX, player.posY
                );

            // El guardia permanece quieto hasta detectar al jugador.
            if (enemy.state == EnemyState::Guard)
            {
                if (!canSeePlayer)
                    continue;

                enemy.state = EnemyState::Chase;
            }

            // Girar suavemente hacia el jugador.
            const double targetAngle = std::atan2(dy, dx);
            const double angleDifference = NormalizeAngle(
                targetAngle - enemy.angle
            );

            const double maxTurn = enemy.turnSpeed * deltaTime;

            enemy.angle += std::clamp(
                angleDifference, -maxTurn, maxTurn
            );

            enemy.angle = NormalizeAngle(enemy.angle);

            // Si perdió de vista al jugador, deja de actuar.
            if (!canSeePlayer)
                continue;

            // Primero se acerca; después puede atacar.
            if (distance > enemy.attackRange)
            {
                const double moveX =
                    std::cos(enemy.angle) *
                    enemy.moveSpeed * deltaTime;

                const double moveY =
                    std::sin(enemy.angle) *
                    enemy.moveSpeed * deltaTime;

                const double nextX = enemy.posX + moveX;
                const double nextY = enemy.posY + moveY;

                // Colisiones independientes por eje.
                if (IsWalkable(nextX, enemy.posY))
                    enemy.posX = nextX;

                if (IsWalkable(enemy.posX, nextY))
                    enemy.posY = nextY;
            }
            else if (enemy.attackCooldown <= 0.0)
            {
                SpawnProjectile(enemy);
                enemy.attackCooldown = enemy.attackInterval;
            }

            
        }
    }

    void UpdateProjectiles(double deltaTime)
    {
        for (Projectile& projectile : projectiles)
        {
            if (!projectile.active)
                continue;

            const double moveX =
                projectile.velocityX * projectile.speed * deltaTime;

            const double moveY =
                projectile.velocityY * projectile.speed * deltaTime;

            const double nextX = projectile.posX + moveX;
            const double nextY = projectile.posY + moveY;

            // Si una pared bloquea el proyectil, desaparece.
            if (!IsWalkable(nextX, nextY))
            {
                projectile.active = false;
                continue;
            }

            projectile.posX = nextX;
            projectile.posY = nextY;

            const double dx = player.posX - projectile.posX;
            const double dy = player.posY - projectile.posY;

            const double distanceSquared = dx * dx + dy * dy;
            const double hitRadius = projectile.radius + 0.20;

            if (distanceSquared <= hitRadius * hitRadius)
            {
                DamagePlayer(projectile.damage);
                projectile.active = false;
            }
        }

        // Retirar proyectiles que ya impactaron o chocaron.
        projectiles.erase(
            std::remove_if(
                projectiles.begin(),
                projectiles.end(),
                [](const Projectile& projectile)
                {
                    return !projectile.active;
                }
            ),
            projectiles.end()
        );
    }


    void RenderMap2D(SDL_Renderer* m_Renderer)
    {
        SDL_SetRenderDrawColor(m_Renderer, 25, 25, 30, 255);
        SDL_RenderClear(m_Renderer);

        for (int y = 0; y < Dream::MapHeight; ++y)
        {
            for (int x = 0; x < Dream::MapWidth; ++x)
            {
                SDL_Rect tile{
                    x * Dream::TileSize,
                    y * Dream::TileSize,
                    Dream::TileSize,
                    Dream::TileSize
                };

                if (Dream::Map[y][x] == 1)
                {
                    SDL_SetRenderDrawColor(
                        m_Renderer, 100, 110, 125, 255
                    );
                }
                else
                {
                    SDL_SetRenderDrawColor(
                        m_Renderer, 215, 215, 205, 255
                    );
                }

                SDL_RenderFillRect(m_Renderer, &tile);

                SDL_SetRenderDrawColor(
                    m_Renderer, 45, 45, 50, 255
                );
                SDL_RenderDrawRect(m_Renderer, &tile);
                }
            }
    }

    void RenderPlayer(SDL_Renderer* m_Renderer)
    {
        constexpr int playerSize = 8;

        SDL_Rect playerRect{
            static_cast<int>(
                player.posX * Dream::TileSize - playerSize / 2
            ),
            static_cast<int>(
                player.posY * Dream::TileSize - playerSize / 2
            ),
            playerSize,
            playerSize
        };

        SDL_SetRenderDrawColor(m_Renderer, 220, 45, 45, 255);

        SDL_RenderFillRect(m_Renderer, &playerRect);

        SDL_SetRenderDrawColor(m_Renderer, 30, 100, 220, 255);

        SDL_RenderDrawLine(
            m_Renderer,
            static_cast<int>(player.posX * Dream::TileSize),
            static_cast<int>(player.posY * Dream::TileSize),
            static_cast<int>(
                (player.posX + player.dirX) * Dream::TileSize
            ),
            static_cast<int>(
                (player.posY + player.dirY) * Dream::TileSize
            )
        );

            
    }
        
    void RenderRays2D(SDL_Renderer* renderer)
    {
        constexpr int rayCount = 61;

        const int startX =
            static_cast<int>(player.posX * Dream::TileSize);

        const int startY =
            static_cast<int>(player.posY * Dream::TileSize);

        for (int i = 0; i < rayCount; ++i)
        {
            const double cameraX =
                2.0 * i / (rayCount - 1) - 1.0;

            const double rayDirX =
                player.dirX + player.planeX * cameraX;

            const double rayDirY =
                player.dirY + player.planeY * cameraX;

            const RayHit hit = CastRay(rayDirX, rayDirY);

            if (!hit.hit)
                continue;

            const int endX = static_cast<int>(
                (player.posX + rayDirX * hit.distance)
                * Dream::TileSize
            );

            const int endY = static_cast<int>(
                (player.posY + rayDirY * hit.distance)
                * Dream::TileSize
            );

            // Colores distintos según la orientación de la pared.
            if (hit.side == 0)
            {
                SDL_SetRenderDrawColor(
                    renderer, 40, 140, 255, 255
                );
            }
            else
            {
                SDL_SetRenderDrawColor(
                    renderer, 255, 170, 40, 255
                );
            }

            SDL_RenderDrawLine(
                renderer,
                startX, startY,
                endX, endY
            );
        }
    }


   
    
    void RenderMap3D(SDL_Renderer* renderer)
    {
        SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
        SDL_RenderClear(renderer);

        // Cielo texturizado
        RenderSky(renderer);


        // Suelo texturizado
        RenderFloor(renderer);

        wallDepthBuffer.resize(Dream::ScreenWidth, std::numeric_limits<double>::infinity());

        // Renderizado de paredes texturizadas
        for (int x = 0; x < Dream::ScreenWidth; ++x)
        {
            const double cameraX =
                2.0 * x / Dream::ScreenWidth - 1.0;

            const double rayDirX =
                player.dirX + player.planeX * cameraX;

            const double rayDirY =
                player.dirY + player.planeY * cameraX;

            const RayHit hit = CastRay(rayDirX, rayDirY);

            wallDepthBuffer[x] = hit.hit ? hit.distance : std::numeric_limits<double>::infinity();

            if (!hit.hit || hit.distance <= 0.0)
                continue;

            // El valor de la celda determina el material.
            if (hit.wallType <= 0 || hit.wallType >= 5)
                continue;

            WallMaterial& material = wallMaterials[hit.wallType];

            if (!material.texture ||
                material.width <= 0 ||
                material.height <= 0)
            {
                // Material no cargado: mostrar una pared de respaldo.
                const double distance = std::max(hit.distance, 0.001);

                const int lineHeight = static_cast<int>(
                    Dream::ScreenHeight / distance
                );

                const int drawStart = std::max(
                    0, Dream::ScreenHeight / 2 - lineHeight / 2
                );

                const int drawEnd = std::min(
                    Dream::ScreenHeight,
                    Dream::ScreenHeight / 2 + lineHeight / 2
                );

                SDL_SetRenderDrawColor(
                    renderer,
                    hit.side == 0 ? 150 : 95,
                    hit.side == 0 ? 150 : 95,
                    hit.side == 0 ? 160 : 105,
                    255
                );

                SDL_RenderDrawLine(
                    renderer, x, drawStart, x, drawEnd
                );

                continue;
            }

            const double distance = std::max(hit.distance, 0.001);

            const int lineHeight = static_cast<int>(
                Dream::ScreenHeight / distance
            );

            if (lineHeight <= 0)
                continue;

            const int originalStart =
                Dream::ScreenHeight / 2 - lineHeight / 2;

            const int originalEnd = originalStart + lineHeight;

            const int drawStart = std::max(0, originalStart);
            const int drawEnd = std::min(
                Dream::ScreenHeight, originalEnd
            );

            if (drawStart >= drawEnd)
                continue;

            // Posición exacta del impacto en la pared.
            double wallX;

            if (hit.side == 0)
            {
                wallX = player.posY + hit.distance * rayDirY;
            }
            else
            {
                wallX = player.posX + hit.distance * rayDirX;
            }

            wallX -= std::floor(wallX);

            int texX = static_cast<int>(wallX * material.width);
            texX = std::min(std::max(texX, 0), material.width - 1);

            // Corregir la orientación de la textura.
            if ((hit.side == 0 && rayDirX > 0.0) ||
                (hit.side == 1 && rayDirY < 0.0))
            {
                texX = material.width - texX - 1;
            }

            // Recorte vertical proporcional para evitar deformaciones.
            const int sourceY = std::min(
                std::max(
                    static_cast<int>(
                        static_cast<long long>(drawStart - originalStart)
                        * material.height / lineHeight
                    ),
                    0
                ),
                material.height - 1
            );

            const int sourceEndY = std::min(
                std::max(
                    static_cast<int>(
                        static_cast<long long>(drawEnd - originalStart)
                        * material.height / lineHeight
                    ),
                    sourceY + 1
                ),
                material.height
            );

        

            SDL_Rect source{
                texX,
                sourceY,
                1,
                sourceEndY - sourceY
            };

            SDL_Rect destination{
                x,
                drawStart,
                1,
                drawEnd - drawStart
            };

            // Iluminación básica por orientación.
            if (hit.side == 0)
            {
                SDL_SetTextureColorMod(
                    material.texture, 175, 175, 190
                );
            }
            else
            {
                SDL_SetTextureColorMod(
                    material.texture, 110, 110, 125
                );
            }

            SDL_RenderCopy(
                renderer,
                material.texture,
                &source,
                &destination
            );

            // SDL_RenderCopy puede fallar; en cualquier caso,
            // restauramos el color en la siguiente iteración.
            SDL_SetTextureColorMod(
                material.texture, 255, 255, 255
            );
        }
    }

    void UpdateAutonomousEnemy(Enemy& enemy, double deltaTime)
    {
        if (!enemy.active)
            return;

        enemy.turnTimer -= deltaTime;
        enemy.moveTimer -= deltaTime;

        // Elegir una nueva orientación de vez en cuando.
        if (enemy.patrolRotation)
        {
            // Rotación continua de 360 grados.
            enemy.angle += enemy.autonomousTurnSpeed * deltaTime;

            // Mantener el ángulo dentro de 0 y 2 PI.
            constexpr double TWO_PI = 6.28318530717958647692;

            if (enemy.angle >= TWO_PI)
                enemy.angle -= TWO_PI;
        }

        // Girar progresivamente hacia la orientación elegida.
        const double difference = NormalizeAngle(
            enemy.targetAngle - enemy.angle
        );

        const double maxTurn =
            enemy.autonomousTurnSpeed * deltaTime;

        enemy.angle += std::clamp(
            difference,
            -maxTurn,
            maxTurn
        );

        enemy.angle = NormalizeAngle(enemy.angle);

        // Elegir cuándo caminar y cuándo descansar.
        if (enemy.moveTimer <= 0.0)
        {
            enemy.moving = !enemy.moving;

            enemy.moveTimer = enemy.moving
                ? 2.0 + (static_cast<double>(std::rand()) / RAND_MAX) * 2.0
                : 1.0 + (static_cast<double>(std::rand()) / RAND_MAX) * 2.0;
        }

        if (!enemy.moving)
            return;

        // Avanzar hacia donde está mirando, no hacia el jugador.
        const double moveX =
            std::cos(enemy.angle) *
            enemy.autonomousMoveSpeed * deltaTime;

        const double moveY =
            std::sin(enemy.angle) *
            enemy.autonomousMoveSpeed * deltaTime;

        const double nextX = enemy.posX + moveX;
        const double nextY = enemy.posY + moveY;

        if (IsWalkable(nextX, enemy.posY))
            enemy.posX = nextX;
        else
            enemy.targetAngle = NormalizeAngle(
                enemy.angle + 1.57079632679
            );

        if (IsWalkable(enemy.posX, nextY))
            enemy.posY = nextY;
        else
            enemy.targetAngle = NormalizeAngle(
                enemy.angle - 1.57079632679
            );
    }

    int GetEnemySpriteIndex(const Enemy& enemy)
    {
        constexpr double PI = 3.14159265358979323846;
        constexpr double SECTOR = PI / 4.0;

        const double angleToPlayer = std::atan2(
            player.posY - enemy.posY,
            player.posX - enemy.posX
        );

        const double relativeAngle = NormalizeAngle(
            angleToPlayer - enemy.angle
        );

        int index = static_cast<int>(
            std::round(relativeAngle / SECTOR)
        );

        index = (index % 8 + 8) % 8;

        return index;
    }

    void RenderEnemies(SDL_Renderer* renderer)
    {
        const int screenWidth = Dream::ScreenWidth;
        const int screenHeight = Dream::ScreenHeight;

        // Ordenar del más lejano al más cercano.
        std::vector<Enemy*> visibleEnemies;

        for (Enemy& enemy : enemies)
        {
            if (enemy.active && enemy.sprites)
                visibleEnemies.push_back(&enemy);
        }

        std::sort(
            visibleEnemies.begin(),
            visibleEnemies.end(),
            [](const Enemy* a, const Enemy* b)
            {
                const double adx = a->posX - player.posX;
                const double ady = a->posY - player.posY;
                const double bdx = b->posX - player.posX;
                const double bdy = b->posY - player.posY;

                return adx * adx + ady * ady >
                    bdx * bdx + bdy * bdy;
            }
        );

        for (const Enemy* enemy : visibleEnemies)
        {
            const int directionIndex = GetEnemySpriteIndex(*enemy);

            const SpriteMaterial& sprite =
                enemy->sprites->directions[directionIndex];

            if (!sprite.texture ||
                sprite.width <= 0 ||
                sprite.height <= 0)
            {
                continue;
            }

            const double spriteX = enemy->posX - player.posX;
            const double spriteY = enemy->posY - player.posY;

            const double determinant =
                player.planeX * player.dirY -
                player.dirX * player.planeY;

            if (std::abs(determinant) < 1e-9)
                continue;

            const double inverseDeterminant = 1.0 / determinant;

            const double transformX = inverseDeterminant *
                (player.dirY * spriteX - player.dirX * spriteY);

            const double transformY = inverseDeterminant *
                (-player.planeY * spriteX + player.planeX * spriteY);

            if (transformY <= 0.05)
                continue;

            const int screenX = static_cast<int>(
                (screenWidth / 2.0) *
                (1.0 + transformX / transformY)
            );

            // Altura de un sprite de un metro de altura.
            const int spriteHeight = std::max(
                1,
                static_cast<int>(std::abs(screenHeight / transformY))
            );

            // Mantener la proporción de la imagen PNG.
            const int spriteWidth = std::max(
                1,
                static_cast<int>(
                    spriteHeight *
                    static_cast<double>(sprite.width) /
                    sprite.height
                )
            );

            const int drawStartX = screenX - spriteWidth / 2;
            const int drawEndX = drawStartX + spriteWidth;

            const int drawStartY = screenHeight / 2 - spriteHeight / 2;
            const int drawEndY = drawStartY + spriteHeight;

            const int clippedStartX = std::max(0, drawStartX);
            const int clippedEndX = std::min(screenWidth, drawEndX);

            for (int stripe = clippedStartX;
                stripe < clippedEndX;
                ++stripe)
            {
                // Comparar la profundidad del enemigo con la pared.
                if (stripe < 0 ||
                    stripe >= static_cast<int>(wallDepthBuffer.size()) ||
                    transformY >= wallDepthBuffer[stripe])
                {
                    continue;
                }

                const int texX = std::clamp(
                    static_cast<int>(
                        (stripe - drawStartX) *
                        static_cast<double>(sprite.width) /
                        spriteWidth
                    ),
                    0,
                    sprite.width - 1
                );

                const int clippedStartY = std::max(0, drawStartY);
                const int clippedEndY = std::min(screenHeight, drawEndY);

                if (clippedStartY >= clippedEndY)
                    continue;

                const int texStartY = std::clamp(
                    static_cast<int>(
                        (clippedStartY - drawStartY) *
                        static_cast<double>(sprite.height) /
                        spriteHeight
                    ),
                    0,
                    sprite.height - 1
                );

                const int texEndY = std::clamp(
                    static_cast<int>(
                        std::ceil(
                            (clippedEndY - drawStartY) *
                            static_cast<double>(sprite.height) /
                            spriteHeight
                        )
                    ),
                    texStartY + 1,
                    sprite.height
                );

                SDL_Rect source{
                    texX,
                    texStartY,
                    1,
                    texEndY - texStartY
                };

                SDL_Rect destination{
                    stripe,
                    clippedStartY,
                    1,
                    clippedEndY - clippedStartY
                };

                SDL_RenderCopy(
                    renderer,
                    sprite.texture,
                    &source,
                    &destination
                );
            }
        }
    }

    void RenderProjectiles(SDL_Renderer* renderer)
    {
        const int screenWidth = Dream::ScreenWidth;
        const int screenHeight = Dream::ScreenHeight;

        SDL_SetRenderDrawColor(renderer, 255, 110, 25, 255);

        for (const Projectile& projectile : projectiles)
        {
            if (!projectile.active)
                continue;

            const double spriteX = projectile.posX - player.posX;
            const double spriteY = projectile.posY - player.posY;

            const double determinant =
                player.planeX * player.dirY -
                player.dirX * player.planeY;

            if (std::abs(determinant) < 1e-9)
                continue;

            const double invDet = 1.0 / determinant;

            const double transformX = invDet *
                (player.dirY * spriteX - player.dirX * spriteY);

            const double transformY = invDet *
                (-player.planeY * spriteX + player.planeX * spriteY);

            if (transformY <= 0.05)
                continue;

            const int screenX = static_cast<int>(
                screenWidth / 2.0 *
                (1.0 + transformX / transformY)
            );

            const int size = std::max(
                2,
                static_cast<int>(
                    screenHeight * projectile.radius / transformY
                )
            );

            const int startX = screenX - size / 2;
            const int endX = screenX + size / 2;

            const int startY = screenHeight / 2 - size / 2;

            for (int x = std::max(0, startX);
                x < std::min(screenWidth, endX);
                ++x)
            {
                if (x >= static_cast<int>(wallDepthBuffer.size()) ||
                    transformY >= wallDepthBuffer[x])
                {
                    continue;
                }

                SDL_Rect rect{
                    x,
                    startY,
                    1,
                    size
                };

                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }





    

    int Window::GameLoop()
    {
        if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) &
            (IMG_INIT_PNG | IMG_INIT_JPG)) == 0)
        {
            SDL_Log("Error inicializando SDL_image: %s", IMG_GetError());
            return 1;
        }

        if (!Dream::LoadSky(
                m_Renderer,
                "../resources/textures/skyboxes/day.png"))
        {
            IMG_Quit();
            return 1;
        }

        // ID 1: pared normal
        // ID 2: ladrillo
        if (!Dream::LoadWallMaterial(m_Renderer, 1, "../resources/textures/walls/wall.jpeg") ||
            !Dream::LoadWallMaterial(m_Renderer, 2, "../resources/textures/walls/brick.jpeg") ||
            !Dream::LoadWallMaterial(m_Renderer, 3, "../resources/textures/walls/smile.png"))
        {
            for (auto& material : Dream::wallMaterials)
            {
                if (material.texture)
                {
                    SDL_DestroyTexture(material.texture);
                    material.texture = nullptr;
                    material.width = 0;
                    material.height = 0;
                }
            }

            if (Dream::skyTexture)
            {
                SDL_DestroyTexture(Dream::skyTexture);
                Dream::skyTexture = nullptr;
            }

            IMG_Quit();
            return 1;
        }

        if (!Dream::LoadFloorTexture(m_Renderer, "../resources/textures/floors/floor.png"))
        {
            return 1;
        }

        if (!Dream::CreateFloorRenderTexture(m_Renderer))
        {
            return 1;
        }

        Uint32 lastTime = SDL_GetTicks();

        Dream::EnemySpriteSet cacodemonSprites;

        if (!Dream::LoadEnemySprites(
                m_Renderer,
                cacodemonSprites,
                "../resources/textures/enemies/cacodemon"))
        {
            for (auto& sprite : cacodemonSprites.directions)
            {
                if (sprite.texture)
                    SDL_DestroyTexture(sprite.texture);
            }

            return 1;
        }

        // IMPORTANTE: cambia estas coordenadas por una celda libre de tu mapa.
        Dream::Enemy cacodemon{};

        cacodemon.posX = 5.5;
        cacodemon.posY = 5.5;
        cacodemon.angle = 0.0;

        cacodemon.health = 100;
        cacodemon.damage = 10;

        cacodemon.aiType = Dream::EnemyAIType::Autonomous;

        cacodemon.autonomousTurnSpeed = 0.8;
        cacodemon.autonomousMoveSpeed = 0.7;

        cacodemon.sprites = &cacodemonSprites;

        // Evitar duplicarlo si reinicializas el nivel.
        Dream::enemies.clear();
        Dream::enemies.push_back(cacodemon);

        while (running)
        {
            const Uint32 currentTime = SDL_GetTicks();

            double deltaTime =
                (currentTime - lastTime) / 1000.0;

            lastTime = currentTime;

            deltaTime = std::min(deltaTime, 0.05);

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                    running = false;
            }

            // Actualizar la simulación.
            Dream::MovementPlayer(deltaTime);
            Dream::UpdateEnemies(deltaTime);
            Dream::UpdateProjectiles(deltaTime);

            // Dibujar el mundo y sus entidades.
            Dream::RenderMap3D(m_Renderer);
            Dream::RenderProjectiles(m_Renderer);
            Dream::RenderEnemies(m_Renderer);

            SDL_RenderPresent(m_Renderer);
        }

        Dream::enemies.clear();
        Dream::projectiles.clear();

        for (auto& sprite : cacodemonSprites.directions)
        {
            if (sprite.texture)
            {
                SDL_DestroyTexture(sprite.texture);
                sprite.texture = nullptr;
            }

            sprite.width = 0;
            sprite.height = 0;
        }

        // Liberar todos los materiales.
        for (auto& material : Dream::wallMaterials)
        {
            if (material.texture)
            {
                SDL_DestroyTexture(material.texture);
                material.texture = nullptr;
            }

            material.width = 0;
            material.height = 0;
        }

        if (Dream::skyTexture)
        {
            SDL_DestroyTexture(Dream::skyTexture);
            Dream::skyTexture = nullptr;
        }

        Dream::skyWidth = 0;
        Dream::skyHeight = 0;



        Destroy();

        IMG_Quit();
        SDL_Quit();

        return 0;
    }
}
