<?php

namespace App\Jobs;

use App\Models\Run;
use App\Services\AuditReportService;
use Illuminate\Bus\Queueable;
use Illuminate\Contracts\Queue\ShouldQueue;
use Illuminate\Foundation\Bus\Dispatchable;
use Illuminate\Queue\InteractsWithQueue;
use Illuminate\Queue\SerializesModels;
use Illuminate\Support\Facades\Log;

class GenerateReports implements ShouldQueue
{
    use Dispatchable, InteractsWithQueue, Queueable, SerializesModels;

    public int $timeout = 600;

    public int $tries = 2;

    public function __construct(
        protected Run $run,
    ) {}

    public function handle(AuditReportService $reportService): void
    {
        Log::info('GenerateReports job started', ['run_id' => $this->run->uuid]);

        try {
            $reportService->generateAll($this->run);

            Log::info('GenerateReports job completed', ['run_id' => $this->run->uuid]);

        } catch (\Throwable $e) {
            Log::error('GenerateReports job failed', [
                'run_id' => $this->run->uuid,
                'error' => $e->getMessage(),
            ]);

            if ($this->attempts() >= $this->tries) {
                $this->fail($e);
            }
        }
    }
}
