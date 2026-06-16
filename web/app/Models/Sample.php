<?php

namespace App\Models;

use App\Models\Concerns\HasPublicUuid;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Database\Eloquent\Relations\HasMany;
use Illuminate\Database\Eloquent\SoftDeletes;

class Sample extends Model
{
    use HasPublicUuid, SoftDeletes;

    protected $fillable = [
        'run_id', 'sample_id', 'role', 'label',
        'fastq_path', 'fastq_size_bytes', 'fastq_sha256',
        'metadata', 'screening_result', 'out_of_panel_flag',
        'out_of_panel_details', 'species_results',
        'evidence_summary', 'total_reads', 'classified_reads',
    ];

    protected function casts(): array
    {
        return [
            'metadata' => 'array',
            'out_of_panel_details' => 'array',
            'species_results' => 'array',
            'evidence_summary' => 'array',
            'out_of_panel_flag' => 'boolean',
            'total_reads' => 'integer',
            'classified_reads' => 'integer',
        ];
    }

    public function run(): BelongsTo
    {
        return $this->belongsTo(Run::class);
    }

    public function reports(): HasMany
    {
        return $this->hasMany(Report::class);
    }

    public function isControl(): bool
    {
        return in_array($this->role, ['positive_control', 'negative_control', 'extraction_blank']);
    }

    public function isTestSample(): bool
    {
        return $this->role === 'sample';
    }
}
