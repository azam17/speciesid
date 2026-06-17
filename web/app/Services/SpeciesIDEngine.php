<?php

namespace App\Services;

use App\Models\Run;
use App\Models\Sample;
use Illuminate\Support\Facades\Log;
use Illuminate\Support\Facades\Storage;
use Opis\JsonSchema\Errors\ErrorFormatter;
use Opis\JsonSchema\Validator;
use Symfony\Component\Process\Process;

class SpeciesIDEngine
{
    protected string $binaryPath;

    protected string $schemaPath;

    public function __construct()
    {
        $this->binaryPath = config('speciesid.engine.binary_path', base_path('../speciesid'));
        $this->schemaPath = config('speciesid.engine.schema_path', base_path('../schema'));
    }

    /**
     * Build the analysis manifest from a Run and its Samples.
     */
    public function buildManifest(Run $run): array
    {
        $run->load('referenceDatabase', 'calibrationProfile', 'samples');

        $samples = $run->samples->map(function (Sample $sample) {
            $metadata = $this->cleanManifestValue($sample->metadata ?? []);
            if ($metadata === []) {
                $metadata = new \stdClass;
            }

            $manifestSample = [
                'sample_id' => $sample->sample_id,
                'role' => $sample->role,
                'fastq_path' => Storage::disk('fastq')->path($sample->fastq_path),
                'metadata' => $metadata,
            ];

            if (is_string($sample->label) && $sample->label !== '') {
                $manifestSample['label'] = $sample->label;
            }

            return $manifestSample;
        })->toArray();

        $analysisParams = $this->cleanManifestValue($run->analysis_params ?? []);
        $runMetadata = $this->cleanManifestValue($run->run_metadata ?? []);
        if ($analysisParams === []) {
            $analysisParams = new \stdClass;
        }
        if ($runMetadata === []) {
            $runMetadata = new \stdClass;
        }

        $manifest = [
            'manifest_version' => '1.0.0',
            'run_id' => (string) $run->uuid,
            'index_path' => Storage::disk('indexes')->path(
                $run->referenceDatabase->index_path
            ),
            'database_hash' => $run->referenceDatabase->sha256_hash,
            'marker_panel' => $run->marker_panel,
            'samples' => $samples,
            'analysis_params' => $analysisParams,
            'run_metadata' => $runMetadata,
            'calibration_profile_path' => $run->calibrationProfile
                ? Storage::disk('calibrations')->path($run->calibrationProfile->file_path)
                : null,
        ];

        $this->validateManifest($manifest);

        return $manifest;
    }

    protected function cleanManifestValue(mixed $value): mixed
    {
        if (is_array($value)) {
            $isList = array_is_list($value);
            $clean = [];

            foreach ($value as $key => $item) {
                $item = $this->cleanManifestValue($item);

                if ($item === null || $item === '' || $item === []) {
                    continue;
                }

                $clean[$key] = $item;
            }

            return $isList ? array_values($clean) : $clean;
        }

        return $value;
    }

    /**
     * Execute the C engine with a manifest JSON file.
     */
    public function execute(Run $run): array
    {
        $manifest = $this->buildManifest($run);

        $manifestPath = $this->writeTempManifest($run, $manifest);
        $resultPath = $this->getResultPath($run);

        $command = [
            $this->binaryPath,
            'analyze',
            '--manifest',
            $manifestPath,
            '--output',
            $resultPath,
        ];

        Log::info('SpeciesID engine starting', [
            'run_id' => $run->uuid,
            'command' => $command,
        ]);

        $startTime = microtime(true);

        $process = new Process($command);
        $process->setTimeout((int) config('speciesid.engine.timeout_seconds', 3600));
        $process->run();

        $exitCode = $process->getExitCode() ?? -1;
        $stderr = trim($process->getErrorOutput());
        $stdout = trim($process->getOutput());

        $duration = microtime(true) - $startTime;

        Log::info('SpeciesID engine finished', [
            'run_id' => $run->uuid,
            'exit_code' => $exitCode,
            'duration_s' => round($duration, 2),
        ]);

        if ($exitCode !== 0) {
            Log::error('SpeciesID engine failed', [
                'run_id' => $run->uuid,
                'stderr' => $stderr,
            ]);

            throw new \RuntimeException(
                "SpeciesID engine exited with code {$exitCode}: {$stderr}"
            );
        }

        if (! file_exists($resultPath)) {
            throw new \RuntimeException('SpeciesID engine did not produce result file');
        }

        $resultJson = file_get_contents($resultPath);
        $resultObject = json_decode($resultJson);

        if (json_last_error() !== JSON_ERROR_NONE) {
            throw new \RuntimeException('Invalid JSON from SpeciesID engine: '.json_last_error_msg());
        }

        $this->validateResult($resultObject);

        // Inject engine metadata if not present
        $resultObject->audit->engine_exit_code = $exitCode;
        $resultObject->audit->engine_stderr = $stderr;
        $resultObject->audit->engine_stdout = $stdout;
        $resultObject->audit->engine_command = implode(' ', array_map('escapeshellarg', $command));
        $resultObject->audit->computation_time_seconds = round($duration, 2);
        $augmentedJson = json_encode($resultObject, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
        file_put_contents($resultPath, $augmentedJson);
        $result = json_decode($augmentedJson, true);

        // Persist result and update run
        $run->update([
            'result_path' => $resultPath,
            'run_qc_summary' => $result['run_qc'] ?? null,
            'total_reads' => $result['run_qc']['total_reads'] ?? null,
            'classified_read_pct' => $result['run_qc']['classified_read_pct'] ?? null,
            'run_qc_status' => $result['run_qc']['status'] ?? 'RUN_FAIL',
            'computation_time_seconds' => round($duration, 2),
            'engine_exit_code' => $exitCode,
            'completed_at' => now(),
            'status' => 'completed',
        ]);

        $this->updateSamples($run, $result);

        return $result;
    }

    /**
     * Update sample records from engine results.
     */
    protected function updateSamples(Run $run, array $result): void
    {
        $sampleResults = collect($result['samples'] ?? [])->keyBy('sample_id');

        foreach ($run->samples as $sample) {
            $sr = $sampleResults->get($sample->sample_id);

            if (! $sr) {
                $sample->update(['screening_result' => 'RUN_FAIL']);

                continue;
            }

            $sample->update([
                'screening_result' => $sr['screening_result'] ?? null,
                'out_of_panel_flag' => $sr['out_of_panel_flag'] ?? false,
                'out_of_panel_details' => $sr['out_of_panel_details'] ?? null,
                'species_results' => $sr['species'] ?? null,
                'evidence_summary' => $sr['evidence_summary'] ?? null,
                'total_reads' => $sr['evidence_summary']['total_reads'] ?? null,
                'classified_reads' => $sr['evidence_summary']['classified_reads'] ?? null,
            ]);
        }
    }

    protected function writeTempManifest(Run $run, array $manifest): string
    {
        $dir = storage_path("app/runs/{$run->uuid}");
        if (! is_dir($dir)) {
            mkdir($dir, 0755, true);
        }

        $path = "{$dir}/manifest.json";
        file_put_contents($path, json_encode($manifest, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));

        $run->update(['manifest_path' => $path]);

        return $path;
    }

    protected function getResultPath(Run $run): string
    {
        $dir = storage_path("app/runs/{$run->uuid}");
        if (! is_dir($dir)) {
            mkdir($dir, 0755, true);
        }

        return "{$dir}/result.json";
    }

    protected function validateManifest(array $manifest): void
    {
        $schemaPath = "{$this->schemaPath}/manifest_v1.json";

        if (! file_exists($schemaPath)) {
            Log::warning('Manifest schema not found, skipping validation', ['path' => $schemaPath]);

            return;
        }

        $schema = json_decode(file_get_contents($schemaPath));
        $data = json_decode(json_encode($manifest));

        $validator = new Validator;
        $result = $validator->validate($data, $schema);

        if (! $result->isValid()) {
            $formatter = new ErrorFormatter;
            $errors = $formatter->formatFlat($result->error());
            $msg = 'Manifest validation failed: '.implode('; ', $errors);
            Log::error($msg);
            throw new \InvalidArgumentException($msg);
        }
    }

    protected function validateResult(mixed $result): void
    {
        $schemaPath = "{$this->schemaPath}/result_v1.json";

        if (! file_exists($schemaPath)) {
            Log::warning('Result schema not found, skipping validation');

            return;
        }

        $schema = json_decode(file_get_contents($schemaPath));
        $data = is_array($result) ? json_decode(json_encode($result)) : $result;

        $validator = new Validator;
        $validationResult = $validator->validate($data, $schema);

        if (! $validationResult->isValid()) {
            $formatter = new ErrorFormatter;
            $errors = $formatter->formatFlat($validationResult->error());
            $msg = 'Result validation failed: '.implode('; ', $errors);
            Log::error($msg, ['errors' => $errors]);
            throw new \UnexpectedValueException($msg);
        }
    }
}
