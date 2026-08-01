Abre el Codespace existente symmetrical telegram y dentro del terminal ejecuta:

git fetch origin
git switch test
git pull origin test

Comprueba:

git branch --show-current

Debe mostrar:

test

Desde ese momento, ese mismo Codespace estará trabajando sobre la rama test.

Cuando quieras volver a main:

git switch main
git pull origin main