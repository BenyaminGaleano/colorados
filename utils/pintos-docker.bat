@echo off

set dockerid=%pintos%\.container-id

if not exist "%pintos%\.container-id" (docker create -t -i -v %pintos%:/root/pintos -v %pintos%\.pintos-docker:/root/.pintos-docker --cidfile %dockerid% pintos)

set /p dockerid=<%dockerid%

for /f %%g in ('docker container list --no-trunc -q') do (set target=%%g)

echo %target% | findstr %dockerid%

if errorlevel 1 (
    echo Iniciando contenedor
    docker start -i %dockerid%
) else (
    echo Ejecutando nueva terminal
    docker exec -it %dockerid% bash
)

