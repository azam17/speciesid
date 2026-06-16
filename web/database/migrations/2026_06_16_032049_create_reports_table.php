<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('reports', function (Blueprint $table) {
            $table->id();
            $table->uuid()->unique();
            $table->foreignId('run_id')->constrained()->cascadeOnDelete();
            $table->foreignId('sample_id')->nullable()->constrained()->nullOnDelete();
            // NULL for run-level reports
            $table->string('report_type')->default('sample');
            // sample / run_summary / audit_package
            $table->string('format')->default('json');
            // json / pdf / tsv / zip
            $table->string('file_path')->nullable();
            $table->string('sha256_hash', 64)->nullable();
            $table->json('content')->nullable();
            $table->integer('generation_attempts')->default(0);
            $table->string('status')->default('pending');
            $table->text('error_message')->nullable();
            $table->timestamp('generated_at')->nullable();
            $table->timestamps();
            $table->index('run_id');
            $table->index('sample_id');
            $table->index('report_type');
            $table->index('status');
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('reports');
    }
};
