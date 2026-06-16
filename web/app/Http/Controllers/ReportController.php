<?php

namespace App\Http\Controllers;

use App\Models\Run;
use App\Models\Sample;
use App\Services\AuditReportService;
use Illuminate\Support\Facades\Gate;

class ReportController extends Controller
{
    public function sampleJson(Sample $sample)
    {
        Gate::authorize('view', $sample);
        if (! $sample->species_results) {
            abort(404, 'Results not available');
        }

        return response()->json([
            'schema_version' => '1.0.0',
            'sample_id' => $sample->sample_id,
            'screening_result' => $sample->screening_result,
            'species' => $sample->species_results,
            'evidence_summary' => $sample->evidence_summary,
            'run_qc_status' => $sample->run->run_qc_status,
            'database_hash' => $sample->run->referenceDatabase->sha256_hash ?? null,
        ]);
    }

    public function sampleTsv(Sample $sample)
    {
        Gate::authorize('view', $sample);
        if (! $sample->species_results) {
            abort(404, 'Results not available');
        }

        $headers = [
            'Content-Type' => 'text/tab-separated-values',
            'Content-Disposition' => "attachment; filename={$sample->sample_id}_speciesid.tsv",
        ];

        $rows = ["species_id\tcommon_name\tcategory\tconfidence_class\tscreening_estimate_pct\tci_lo_pct\tci_hi_pct\tp_value\tcontrol_adjusted\tambiguity_group\tconfirmatory_recommended"];

        foreach ($sample->species_results as $sp) {
            $rows[] = implode("\t", [
                $sp['species_id'] ?? '',
                $sp['common_name'] ?? '',
                $sp['species_category'] ?? '',
                $sp['confidence_class'] ?? '',
                $sp['screening_estimate_pct'] ?? '',
                $sp['ci_lo_pct'] ?? '',
                $sp['ci_hi_pct'] ?? '',
                $sp['p_value'] ?? '',
                $sp['control_adjusted'] ? 'yes' : 'no',
                implode(';', $sp['ambiguity_group'] ?? []),
                $sp['confirmatory_recommended'] ? 'yes' : 'no',
            ]);
        }

        return response(implode("\n", $rows), 200, $headers);
    }

    public function samplePdf(Sample $sample, AuditReportService $reportService)
    {
        Gate::authorize('view', $sample);
        $pdf = $reportService->generateSamplePdf($sample);

        return response($pdf, 200, [
            'Content-Type' => 'application/pdf',
            'Content-Disposition' => "inline; filename={$sample->sample_id}_report.pdf",
        ]);
    }

    public function runZip(Run $run, AuditReportService $reportService)
    {
        Gate::authorize('view', $run);
        $zipPath = $reportService->generateRunZip($run);

        return response()->download($zipPath);
    }
}
