# SpeciesID — Laravel Forge Deployment Configuration

## Resource Model

SpeciesID does not run analysis in the visitor's browser. The C engine runs wherever
the Laravel application is installed.

- **Lab-local / on-prem install:** Laravel, SQLite/PostgreSQL, local storage, and
  the `speciesid` binary run on the lab workstation or lab server. This is the
  recommended mode when each lab must use its own local compute and files.
- **Hosted Forge install:** Laravel runs on the Forge server, so analysis uses
  the Forge server's CPU, disk, database, and queue workers. User laptops only
  upload files and view reports.

For a single lab workstation, use the default `.env.example` local mode:
`DB_CONNECTION=sqlite`, `FILESYSTEM_DISK=local`, and `QUEUE_CONNECTION=sync`.
Redis/Supervisor/PostgreSQL are optional server scaling choices, not required
for local lab use.

## Forge Setup

### 1. Server Requirements
- PHP 8.3+
- PostgreSQL 16+ (recommended) or MySQL 8.0+
- Redis 7+ (for queues)
- Nginx
- Supervisor
- C build tools (gcc, cmake, zlib-dev) for compiling the speciesid binary

### 2. Site Configuration

**Root Directory:** `/public` (Laravel default)

**Nginx Configuration** — add to Forge's "Edit Nginx Configuration":

```nginx
# Increase upload size for FASTQ files (up to 2GB)
client_max_body_size 2048M;
client_body_timeout 600s;

# Proxy WebSocket for Livewire polling (optional)
location /livewire {
    proxy_pass http://127.0.0.1:8080;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";
    proxy_set_header Host $host;
    proxy_read_timeout 3600s;
}
```

### 3. Environment Variables (.env)

Add these to Forge's environment editor:

```
# SpeciesID Engine
SPECIESID_BINARY_PATH="/home/forge/speciesid/speciesid"
SPECIESID_SCHEMA_PATH="/home/forge/speciesid/schema"

# Queue
QUEUE_CONNECTION=redis

# Database (PostgreSQL recommended)
DB_CONNECTION=pgsql
DB_HOST=127.0.0.1
DB_PORT=5432
DB_DATABASE=speciesid
DB_USERNAME=forge
DB_PASSWORD=***

# Redis
REDIS_HOST=127.0.0.1
REDIS_PASSWORD=null
REDIS_PORT=6379

# File Storage
FILESYSTEM_DISK=local

# Session/Auth
SESSION_DRIVER=redis
```

### 4. Deploy Script (Forge "Deploy Script")

```bash
cd /home/forge/speciesid

# Pull latest
git pull origin main

# Install dependencies
$FORGE_COMPOSER install --no-dev --no-interaction --prefer-dist --optimize-autoloader

# Build C engine
cd /home/forge/speciesid
make clean && make

# Build index (first deploy or after database changes)
if [ ! -f storage/app/indexes/default.idx ]; then
    mkdir -p storage/app/databases storage/app/indexes storage/app/calibrations
    ./speciesid build-db -o storage/app/databases/default.db
    ./speciesid index -d storage/app/databases/default.db -o storage/app/indexes/default.idx
fi

# Laravel deploy
cd /home/forge/speciesid/web
php artisan migrate --force
php artisan config:cache
php artisan route:cache
php artisan view:cache

# Restart queue workers
sudo supervisorctl restart speciesid-worker:*
```

### 5. Queue Worker (Supervisor)

Create `/etc/supervisor/conf.d/speciesid-worker.conf` via Forge's "Daemon" UI:

```
[program:speciesid-worker]
process_name=%(program_name)s_%(process_num)02d
command=php /home/forge/speciesid/web/artisan queue:work redis --sleep=3 --tries=2 --max-time=3600 --timeout=3600
autostart=true
autorestart=true
stopasgroup=true
killasgroup=true
user=forge
numprocs=2
redirect_stderr=true
stdout_logfile=/home/forge/.forge/speciesid-worker.log
stopwaitsecs=3600
```

### 6. Database Scaling Notes

**For single-lab / small deployments:**
- PostgreSQL on the same Forge server works fine up to ~100 runs/day
- JSONB columns efficiently store species_results and evidence_summary
- No additional setup needed

**When scaling (100+ users / 1000+ runs/day):**
1. **Separate database server**: Provision a dedicated PostgreSQL server via Forge
2. **Read replicas**: Add read replica for dashboard queries, route writes to primary
3. **JSONB indexing**: Add GIN indexes on the JSONB columns for faster searching:
   ```sql
   CREATE INDEX idx_samples_species_results ON samples USING GIN (species_results jsonb_path_ops);
   CREATE INDEX idx_runs_qc_summary ON runs USING GIN (run_qc_summary jsonb_path_ops);
   ```
4. **Queue scaling**: Increase `numprocs` in supervisor config and add more queue workers
5. **FASTQ storage**: Move to S3-compatible storage (`FILESYSTEM_DISK=s3`)
6. **Result archiving**: Archive runs older than 90 days to cold storage, keep metadata in DB

**PostgreSQL > MySQL for this use case** because:
- Superior JSONB performance and indexing
- Better concurrent read/write handling
- GIN indexes on JSONB for species search queries
- Laravel's PostgreSQL driver is mature and well-tested
