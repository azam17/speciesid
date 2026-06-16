<?php

namespace App\Http\Controllers;

use App\Jobs\AnalyzeRun;
use App\Models\CalibrationProfile;
use App\Models\ReferenceDatabase;
use App\Models\Run;
use App\Models\Sample;
use Illuminate\Http\Request;
use Illuminate\Support\Str;
use Illuminate\Validation\ValidationException;

class UploadController extends Controller
{
    public function create()
    {
        $databases = ReferenceDatabase::where('is_active', true)->get();
        $calibrations = CalibrationProfile::where('is_active', true)->get();

        return view('upload.create', compact('databases', 'calibrations'));
    }

    public function store(Request $request)
    {
        $validated = $request->validate([
            'label' => 'nullable|string|max:255',
            'reference_database_id' => 'required|exists:reference_databases,id',
            'calibration_profile_id' => 'nullable|exists:calibration_profiles,id',
            'marker_panel' => 'required|array|min:1',
            'marker_panel.*' => 'string|in:COI,cytb,12S,16S,NADH2,NADH4,NADH5',
            'detection_threshold' => 'nullable|numeric|min:0|max:1',
            'n_bootstrap' => 'nullable|integer|min:0|max:10000',
            'read_mode' => 'nullable|string|in:auto,short,long',
            'control_adjustment' => 'nullable|boolean',
            'out_of_panel_check' => 'nullable|boolean',
            'resolvability_check' => 'nullable|boolean',
            'operator' => 'nullable|string|max:255',
            'laboratory' => 'nullable|string|max:255',
            'instrument_id' => 'nullable|string|max:255',
            'pcr_cycles' => 'nullable|integer',
            'library_prep_kit' => 'nullable|string|max:255',
            'sequencing_run_id' => 'nullable|string|max:255',
            'notes' => 'nullable|string',
            'samples' => 'required|array|min:1',
            'samples.*.fastq' => 'required|file|extensions:fq,fastq,gz|max:2048000',
            'samples.*.role' => 'required|string|in:sample,positive_control,negative_control,extraction_blank',
            'samples.*.label' => 'nullable|string|max:255',
            'samples.*.product_type' => 'nullable|string|max:255',
            'samples.*.claimed_species' => 'nullable|string|max:500',
            'samples.*.expected_species' => 'nullable|string|max:500',
            'samples.*.batch_id' => 'nullable|string|max:255',
            'samples.*.extraction_kit' => 'nullable|string|max:255',
            'samples.*.extraction_date' => 'nullable|date',
            'samples.*.sequencing_platform' => 'nullable|string|max:255',
        ]);

        ReferenceDatabase::findOrFail($validated['reference_database_id']);

        $hasTestSample = collect($validated['samples'])
            ->contains(fn ($sample) => $sample['role'] === 'sample');
        if (! $hasTestSample) {
            throw ValidationException::withMessages([
                'samples' => 'At least one uploaded FASTQ must be assigned the test sample role.',
            ]);
        }

        $run = Run::create([
            'user_id' => $request->user()->id,
            'reference_database_id' => $validated['reference_database_id'],
            'calibration_profile_id' => $validated['calibration_profile_id'] ?? null,
            'label' => $validated['label'] ?? ('Run '.now()->format('Y-m-d H:i')),
            'marker_panel' => $validated['marker_panel'],
            'analysis_params' => [
                'detection_threshold' => $validated['detection_threshold'] ?? 0.001,
                'n_bootstrap' => $validated['n_bootstrap'] ?? 0,
                'read_mode' => $validated['read_mode'] ?? 'auto',
                'control_adjustment' => $validated['control_adjustment'] ?? true,
                'out_of_panel_check' => $validated['out_of_panel_check'] ?? true,
                'resolvability_check' => $validated['resolvability_check'] ?? true,
            ],
            'run_metadata' => [
                'operator' => $validated['operator'] ?? null,
                'laboratory' => $validated['laboratory'] ?? null,
                'instrument_id' => $validated['instrument_id'] ?? null,
                'pcr_cycles' => $validated['pcr_cycles'] ?? null,
                'library_prep_kit' => $validated['library_prep_kit'] ?? null,
                'sequencing_run_id' => $validated['sequencing_run_id'] ?? null,
                'notes' => $validated['notes'] ?? null,
            ],
            'status' => 'pending',
        ]);

        foreach ($validated['samples'] as $i => $sampleData) {
            $file = $sampleData['fastq'];
            $filename = Str::uuid().'.'.$file->getClientOriginalExtension();
            $storedPath = $file->storeAs(
                "runs/{$run->uuid}/fastq",
                $filename,
                'fastq'
            );

            $hash = hash_file('sha256', $file->getRealPath());

            Sample::create([
                'run_id' => $run->id,
                'sample_id' => $sampleData['role'].'_'.($i + 1),
                'role' => $sampleData['role'],
                'label' => $sampleData['label'] ?? null,
                'fastq_path' => $storedPath,
                'fastq_size_bytes' => $file->getSize(),
                'fastq_sha256' => $hash,
                'metadata' => [
                    'product_type' => $sampleData['product_type'] ?? null,
                    'claimed_species' => isset($sampleData['claimed_species'])
                        ? array_map('trim', explode(',', $sampleData['claimed_species']))
                        : null,
                    'expected_species' => isset($sampleData['expected_species'])
                        ? array_values(array_filter(array_map('trim', explode(',', $sampleData['expected_species']))))
                        : null,
                    'batch_id' => $sampleData['batch_id'] ?? null,
                    'extraction_kit' => $sampleData['extraction_kit'] ?? null,
                    'extraction_date' => $sampleData['extraction_date'] ?? null,
                    'sequencing_platform' => $sampleData['sequencing_platform'] ?? null,
                ],
            ]);
        }

        $run->update(['status' => 'queued']);
        AnalyzeRun::dispatch($run);

        return redirect()->route('runs.show', $run)
            ->with('success', 'Run queued. Analysis will begin shortly.');
    }
}
