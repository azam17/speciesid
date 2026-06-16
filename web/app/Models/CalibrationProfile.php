<?php

namespace App\Models;

use App\Models\Concerns\HasPublicUuid;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Database\Eloquent\Relations\HasMany;
use Illuminate\Database\Eloquent\SoftDeletes;

class CalibrationProfile extends Model
{
    use HasPublicUuid, SoftDeletes;

    protected $fillable = [
        'name', 'version', 'file_path', 'sha256_hash',
        'reference_database_id', 'd_mu', 'd_sigma',
        'b_mu', 'b_sigma', 'n_calibration_samples',
        'residuals', 'is_locked', 'is_active', 'notes',
    ];

    protected function casts(): array
    {
        return [
            'd_mu' => 'float',
            'd_sigma' => 'float',
            'b_mu' => 'float',
            'b_sigma' => 'float',
            'residuals' => 'array',
            'is_locked' => 'boolean',
            'is_active' => 'boolean',
        ];
    }

    public function referenceDatabase(): BelongsTo
    {
        return $this->belongsTo(ReferenceDatabase::class);
    }

    public function runs(): HasMany
    {
        return $this->hasMany(Run::class);
    }
}
