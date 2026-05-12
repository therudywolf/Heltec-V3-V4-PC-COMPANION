# Git: удалённый репозиторий и пуш

## Текущий remote

- **origin:** `https://github.com/therudywolf/Heltec-V3-V4-PC-COMPANION.git`
- Проверка: `git remote -v`

## Если не получается пушить

### 1. Detached HEAD

Если `git branch` показывает `* (HEAD detached from ...)`, вы не на ветке — пуш по умолчанию не обновит ни `main`, ни `WolfPet`.

**Вариант A — перейти на ветку и подтянуть текущий коммит:**

```bash
git checkout WolfPet
git merge 7300c0f
git push origin WolfPet
```

(замените `7300c0f` на нужный коммит, например `git log -1 --oneline` когда HEAD на нём).

**Вариант B — отправить текущий коммит в ветку без переключения:**

```bash
git push origin HEAD:WolfPet
```

Так вы отправите текущий (detached) коммит в удалённую ветку `WolfPet`.

### 2. Доступ к GitHub

- Если ошибка 403/401: проверьте логин и токен (GitHub больше не принимает пароль по HTTPS; нужен Personal Access Token или SSH).
- SSH: замените URL на `git@github.com:therudywolf/Heltec-V3-V4-PC-COMPANION.git` и настройте ключ: `git remote set-url origin git@github.com:...`

### 3. Проверка связи с remote

```bash
git fetch origin
```

Если команда проходит без ошибок, remote доступен; проблема скорее в ветке или правах на push.
