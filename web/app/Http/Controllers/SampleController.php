<?php

namespace App\Http\Controllers;

use App\Models\Sample;
use Illuminate\Support\Facades\Gate;

class SampleController extends Controller
{
    public function show(Sample $sample)
    {
        Gate::authorize('view', $sample);
        $sample->load('run.referenceDatabase', 'run.calibrationProfile');

        if (! $sample->species_results) {
            return view('samples.show_pending', compact('sample'));
        }

        return view('samples.show', compact('sample'));
    }

    public function json(Sample $sample)
    {
        Gate::authorize('view', $sample);
        if (! $sample->species_results) {
            return response()->json(['error' => 'Results not yet available'], 404);
        }

        return response()->json([
            'sample_id' => $sample->sample_id,
            'role' => $sample->role,
            'screening_result' => $sample->screening_result,
            'out_of_panel_flag' => $sample->out_of_panel_flag,
            'out_of_panel_details' => $sample->out_of_panel_details,
            'species' => $sample->species_results,
            'evidence_summary' => $sample->evidence_summary,
            'run' => [
                'run_id' => $sample->run->uuid,
                'status' => $sample->run->status,
                'run_qc_status' => $sample->run->run_qc_status,
                'database_hash' => $sample->run->referenceDatabase->sha256_hash ?? null,
            ],
        ]);
    }
}
