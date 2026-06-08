#!/bin/bash

HOST="127.0.0.1"
PORT="3306"
USER="root"
PASSWORD="password"

BACKUP_DIR="./backup_$(date +%Y%m%d_%H%M%S)"

mkdir -p "$BACKUP_DIR"

DATABASES=(
  "account"
  "common"
  "log"
  "hotbackup"
  "player"
)

for DB in "${DATABASES[@]}"; do
  echo "Dumping $DB..."

  mysqldump \
    -h "$HOST" \
    -P "$PORT" \
    -u "$USER" \
    -p"$PASSWORD" \
    --single-transaction \
    --quick \
    --routines \
    --events \
    --set-gtid-purged=OFF \
    "$DB" > "$BACKUP_DIR/${DB}.sql"

  if [ $? -eq 0 ]; then
    echo "✓ $DB erfolgreich gesichert"
  else
    echo "✗ Fehler beim Dump von $DB"
  fi
done

echo ""
echo "Backups gespeichert unter:"
echo "$BACKUP_DIR"