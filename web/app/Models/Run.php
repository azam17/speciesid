<?php

namespace App\Models;

use App\Models\Concerns\HasPublicUuid;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Database\Eloquent\Relations\HasMany;
use Illuminate\Database\Eloquent\SoftDeletes;

class Run extends Model
{
    use HasPublicUuid, SoftDeletes;

    protected $fillable = [
        'user_id', 'reference_database_id', 'calibration_profile_id',
        'status', 'run_qc_status', 'label', 'marker_panel',
        'analysis_params', 'run_metadata', 'manifest_path',
        'result_path', 'run_qc_summary', 'error_message',
        'total_reads', 'classified_read_pct', 'computation_time_seconds',
        'engine_exit_code', 'started_at', 'completed_at',
    ];

    protected function casts(): array
    {
        return [
            'marker_panel' => 'array',
            'analysis_params' => 'array',
            'run_metadata' => 'array',
            'run_qc_summary' => 'array',
            'started_at' => 'datetime',
            'completed_at' => 'datetime',
            'total_reads' => 'integer',
            'classified_read_pct' => 'float',
            'computation_time_seconds' => 'float',
            'engine_exit_code' => 'integer',
        ];
    }

    public function user(): BelongsTo
    {
        return $this->belongsTo(User::class);
    }

    public function referenceDatabase(): BelongsTo
    {
        return $this->belongsTo(ReferenceDatabase::class);
    }

    public function calibrationProfile(): BelongsTo
    {
        return $this->belongsTo(CalibrationProfile::class);
    }

    public function samples(): HasMany
    {
        return $this->hasMany(Sample::class);
    }

    public function reports(): HasMany
    {
        return $this->hasMany(Report::class);
    }

    public function isComplete(): bool
    {
        return $this->status === 'completed';
    }

    public function isFailed(): bool
    {
        return $this->status === 'failed';
    }
}
