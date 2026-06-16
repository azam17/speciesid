<?php

namespace App\Models;

use App\Models\Concerns\HasPublicUuid;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\HasMany;
use Illuminate\Database\Eloquent\SoftDeletes;

class ReferenceDatabase extends Model
{
    use HasPublicUuid, SoftDeletes;

    protected $fillable = [
        'name', 'version', 'file_path', 'index_path',
        'sha256_hash', 'marker_panel', 'species_list',
        'n_species', 'n_markers', 'resolvability_matrix',
        'is_active', 'notes',
    ];

    protected function casts(): array
    {
        return [
            'marker_panel' => 'array',
            'species_list' => 'array',
            'resolvability_matrix' => 'array',
            'is_active' => 'boolean',
        ];
    }

    public function calibrationProfiles(): HasMany
    {
        return $this->hasMany(CalibrationProfile::class);
    }

    public function runs(): HasMany
    {
        return $this->hasMany(Run::class);
    }
}
