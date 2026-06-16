<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('runs', function (Blueprint $table) {
            $table->id();
            $table->uuid()->unique();
            $table->foreignId('user_id')->constrained()->cascadeOnDelete();
            $table->foreignId('reference_database_id')->constrained()->cascadeOnDelete();
            $table->foreignId('calibration_profile_id')->nullable()->constrained()->nullOnDelete();
            $table->string('status')->default('pending');
            // pending -> queued -> processing -> completed / failed
            $table->string('run_qc_status')->nullable();
            // RUN_PASS / RUN_REVIEW / RUN_FAIL
            $table->string('label')->nullable();
            $table->json('marker_panel');
            $table->json('analysis_params')->nullable();
            $table->json('run_metadata')->nullable();
            $table->string('manifest_path')->nullable();
            $table->string('result_path')->nullable();
            $table->json('run_qc_summary')->nullable();
            $table->text('error_message')->nullable();
            $table->integer('total_reads')->nullable();
            $table->double('classified_read_pct')->nullable();
            $table->double('computation_time_seconds')->nullable();
            $table->integer('engine_exit_code')->nullable();
            $table->timestamp('started_at')->nullable();
            $table->timestamp('completed_at')->nullable();
            $table->timestamps();
            $table->softDeletes();
            $table->index('status');
            $table->index('user_id');
            $table->index('created_at');
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('runs');
    }
};
