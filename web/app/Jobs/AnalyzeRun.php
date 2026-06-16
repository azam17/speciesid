<?php

namespace App\Jobs;

use App\Models\Run;
use App\Services\SpeciesIDEngine;
use Illuminate\Bus\Queueable;
use Illuminate\Contracts\Queue\ShouldQueue;
use Illuminate\Foundation\Bus\Dispatchable;
use Illuminate\Queue\InteractsWithQueue;
use Illuminate\Queue\SerializesModels;
use Illuminate\Support\Facades\Log;

class AnalyzeRun implements ShouldQueue
{
    use Dispatchable, InteractsWithQueue, Queueable, SerializesModels;

    public int $timeout = 3600; // 1 hour max

    public int $tries = 2;

    public function __construct(
        protected Run $run,
    ) {}

    public function handle(SpeciesIDEngine $engine): void
    {
        Log::info('AnalyzeRun job started', [
            'run_id' => $this->run->uuid,
            'job_id' => $this->job?->getJobId(),
        ]);

        $this->run->update([
            'status' => 'processing',
            'started_at' => now(),
        ]);

        try {
            $result = $engine->execute($this->run);

            Log::info('AnalyzeRun job completed', [
                'run_id' => $this->run->uuid,
                'qc_status' => $result['run_qc']['status'] ?? 'unknown',
                'n_samples' => count($result['samples'] ?? []),
            ]);

            // Dispatch report generation
            GenerateReports::dispatch($this->run);

        } catch (\Throwable $e) {
            Log::error('AnalyzeRun job failed', [
                'run_id' => $this->run->uuid,
                'error' => $e->getMessage(),
                'trace' => $e->getTraceAsString(),
            ]);

            $this->run->update([
                'status' => 'failed',
                'error_message' => $e->getMessage(),
                'completed_at' => now(),
            ]);

            // Retry once, then fail permanently
            if ($this->attempts() >= $this->tries) {
                $this->fail($e);
            } else {
                $this->release(30); // wait 30s before retry
            }
        }
    }

    public function failed(\Throwable $e): void
    {
        Log::error('AnalyzeRun job permanently failed', [
            'run_id' => $this->run->uuid,
            'error' => $e->getMessage(),
        ]);
    }
}
