<?php

namespace App\Models;

use App\Models\Concerns\HasPublicUuid;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;

class Report extends Model
{
    use HasPublicUuid;

    protected $fillable = [
        'run_id', 'sample_id', 'report_type', 'format',
        'file_path', 'sha256_hash', 'content',
        'generation_attempts', 'status', 'error_message',
        'generated_at',
    ];

    protected function casts(): array
    {
        return [
            'content' => 'array',
            'generated_at' => 'datetime',
        ];
    }

    public function run(): BelongsTo
    {
        return $this->belongsTo(Run::class);
    }

    public function sample(): BelongsTo
    {
        return $this->belongsTo(Sample::class);
    }

    public function isGenerated(): bool
    {
        return $this->status === 'generated';
    }
}
