<?php

namespace App\Services;

use App\Models\Report;
use App\Models\Run;
use App\Models\Sample;
use Barryvdh\DomPDF\Facade\Pdf;
use Illuminate\Support\Facades\Log;
use Illuminate\Support\Facades\Storage;

class AuditReportService
{
    protected function hashStorageFile(string $path): ?string
    {
        return Storage::exists($path) ? hash_file('sha256', Storage::path($path)) : null;
    }

    /**
     * Generate all report formats for a completed run.
     */
    public function generateAll(Run $run): void
    {
        $run->load('samples');

        // Generate per-sample reports
        foreach ($run->samples as $sample) {
            if ($sample->species_results) {
                $this->generateSampleReports($sample);
            }
        }

        // Generate run summary
        $this->generateRunSummary($run);

        // Generate ZIP evidence package
        $this->generateRunZip($run);
    }

    protected function generateSampleReports(Sample $sample): void
    {
        // JSON report
        $jsonPath = $this->writeSampleJson($sample);
        Report::create([
            'run_id' => $sample->run_id,
            'sample_id' => $sample->id,
            'report_type' => 'sample',
            'format' => 'json',
            'file_path' => $jsonPath,
            'sha256_hash' => $this->hashStorageFile($jsonPath),
            'status' => 'generated',
            'generated_at' => now(),
        ]);

        // TSV report
        $tsvPath = $this->writeSampleTsv($sample);
        Report::create([
            'run_id' => $sample->run_id,
            'sample_id' => $sample->id,
            'report_type' => 'sample',
            'format' => 'tsv',
            'file_path' => $tsvPath,
            'sha256_hash' => $this->hashStorageFile($tsvPath),
            'status' => 'generated',
            'generated_at' => now(),
        ]);

        // PDF report
        $pdfPath = $this->writeSamplePdf($sample);
        Report::create([
            'run_id' => $sample->run_id,
            'sample_id' => $sample->id,
            'report_type' => 'sample',
            'format' => 'pdf',
            'file_path' => $pdfPath,
            'sha256_hash' => $this->hashStorageFile($pdfPath),
            'status' => 'generated',
            'generated_at' => now(),
        ]);
    }

    protected function generateRunSummary(Run $run): void
    {
        $data = [
            'schema_version' => '1.0.0',
            'run_id' => $run->uuid,
            'status' => $run->status,
            'run_qc_status' => $run->run_qc_status,
            'run_qc_summary' => $run->run_qc_summary,
            'created_at' => $run->created_at->toIso8601String(),
            'completed_at' => $run->completed_at?->toIso8601String(),
            'database_hash' => $run->referenceDatabase->sha256_hash ?? null,
            'calibration_hash' => $run->calibrationProfile->sha256_hash ?? null,
            'samples' => $run->samples->map(fn ($s) => [
                'sample_id' => $s->sample_id,
                'role' => $s->role,
                'screening_result' => $s->screening_result,
                'fastq_sha256' => $s->fastq_sha256,
                'fastq_size_bytes' => $s->fastq_size_bytes,
            ]),
        ];

        $path = "runs/{$run->uuid}/reports/run_summary.json";
        Storage::put($path, json_encode($data, JSON_PRETTY_PRINT));

        Report::create([
            'run_id' => $run->id,
            'report_type' => 'run_summary',
            'format' => 'json',
            'file_path' => $path,
            'sha256_hash' => $this->hashStorageFile($path),
            'content' => $data,
            'status' => 'generated',
            'generated_at' => now(),
        ]);
    }

    public function generateRunZip(Run $run): string
    {
        $run->load('samples.reports');
        $zipRelPath = "runs/{$run->uuid}/evidence_package.zip";
        $zipPath = Storage::path($zipRelPath);
        if (! is_dir(dirname($zipPath))) {
            mkdir(dirname($zipPath), 0755, true);
        }

        $zip = new \ZipArchive;
        if ($zip->open($zipPath, \ZipArchive::CREATE | \ZipArchive::OVERWRITE) !== true) {
            throw new \RuntimeException('Cannot create ZIP file');
        }

        // Add run summary
        $summaryPath = "runs/{$run->uuid}/reports/run_summary.json";
        if (Storage::exists($summaryPath)) {
            $zip->addFile(Storage::path($summaryPath), 'run_summary.json');
        }

        // Add each sample's reports
        foreach ($run->samples as $sample) {
            $prefix = "samples/{$sample->sample_id}/";

            // JSON
            $jsonReport = $sample->reports->where('format', 'json')->first();
            if ($jsonReport && $jsonReport->file_path && Storage::exists($jsonReport->file_path)) {
                $zip->addFile(Storage::path($jsonReport->file_path), "{$prefix}{$sample->sample_id}.json");
            }

            // TSV
            $tsvReport = $sample->reports->where('format', 'tsv')->first();
            if ($tsvReport && $tsvReport->file_path && Storage::exists($tsvReport->file_path)) {
                $zip->addFile(Storage::path($tsvReport->file_path), "{$prefix}{$sample->sample_id}.tsv");
            }

            // PDF
            $pdfReport = $sample->reports->where('format', 'pdf')->first();
            if ($pdfReport && $pdfReport->file_path && Storage::exists($pdfReport->file_path)) {
                $zip->addFile(Storage::path($pdfReport->file_path), "{$prefix}{$sample->sample_id}.pdf");
            }
        }

        // Add manifest
        if ($run->manifest_path && file_exists($run->manifest_path)) {
            $zip->addFile($run->manifest_path, 'manifest.json');
        }

        // Add result
        if ($run->result_path && file_exists($run->result_path)) {
            $zip->addFile($run->result_path, 'result.json');
        }

        $zip->close();

        Report::updateOrCreate(
            ['run_id' => $run->id, 'report_type' => 'audit_package', 'format' => 'zip'],
            [
                'file_path' => "runs/{$run->uuid}/evidence_package.zip",
                'sha256_hash' => hash_file('sha256', $zipPath),
                'status' => 'generated',
                'generated_at' => now(),
            ]
        );

        Log::info('Evidence package generated', ['run_id' => $run->uuid, 'path' => $zipPath]);

        return $zipPath;
    }

    protected function writeSampleJson(Sample $sample): string
    {
        $path = "runs/{$sample->run->uuid}/reports/{$sample->sample_id}.json";
        $data = [
            'schema_version' => '1.0.0',
            'sample_id' => $sample->sample_id,
            'role' => $sample->role,
            'screening_result' => $sample->screening_result,
            'out_of_panel_flag' => $sample->out_of_panel_flag,
            'out_of_panel_details' => $sample->out_of_panel_details,
            'species' => $sample->species_results,
            'evidence_summary' => $sample->evidence_summary,
            'run_qc_status' => $sample->run->run_qc_status,
            'database_hash' => $sample->run->referenceDatabase->sha256_hash ?? null,
            'calibration_hash' => $sample->run->calibrationProfile->sha256_hash ?? null,
            'fastq_sha256' => $sample->fastq_sha256,
            'fastq_size_bytes' => $sample->fastq_size_bytes,
        ];
        Storage::put($path, json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

        return $path;
    }

    protected function writeSampleTsv(Sample $sample): string
    {
        $path = "runs/{$sample->run->uuid}/reports/{$sample->sample_id}.tsv";
        $lines = ["species_id\tcommon_name\tcategory\tconfidence_class\tscreening_estimate_pct\tci_lo_pct\tci_hi_pct\tp_value\tcontrol_adjusted"];

        foreach ($sample->species_results ?? [] as $sp) {
            $lines[] = implode("\t", [
                $sp['species_id'] ?? '',
                $sp['common_name'] ?? '',
                $sp['species_category'] ?? '',
                $sp['confidence_class'] ?? '',
                $sp['screening_estimate_pct'] ?? '',
                $sp['ci_lo_pct'] ?? '',
                $sp['ci_hi_pct'] ?? '',
                $sp['p_value'] ?? '',
                ($sp['control_adjusted'] ?? false) ? 'yes' : 'no',
            ]);
        }

        Storage::put($path, implode("\n", $lines));

        return $path;
    }

    protected function writeSamplePdf(Sample $sample): string
    {
        $path = "runs/{$sample->run->uuid}/reports/{$sample->sample_id}.pdf";
        $pdf = $this->generateSamplePdf($sample);
        Storage::put($path, $pdf);

        return $path;
    }

    public function generateSamplePdf(Sample $sample): string
    {
        $sample->load('run.referenceDatabase', 'run.calibrationProfile');

        $data = [
            'sample' => $sample,
            'species' => $sample->species_results ?? [],
            'summary' => $sample->evidence_summary ?? [],
            'run_qc' => $sample->run->run_qc_status ?? 'N/A',
            'generated_at' => now()->toIso8601String(),
            'database' => $sample->run->referenceDatabase,
            'calibration' => $sample->run->calibrationProfile,
        ];

        $pdf = Pdf::loadView('reports.sample_pdf', $data);
        $pdf->setPaper('A4');

        return $pdf->output();
    }
}
