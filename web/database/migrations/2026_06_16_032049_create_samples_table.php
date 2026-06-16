<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('samples', function (Blueprint $table) {
            $table->id();
            $table->uuid()->unique();
            $table->foreignId('run_id')->constrained()->cascadeOnDelete();
            $table->string('sample_id');
            $table->string('role');
            // sample / positive_control / negative_control / extraction_blank
            $table->string('label')->nullable();
            $table->string('fastq_path');
            $table->bigInteger('fastq_size_bytes')->nullable();
            $table->string('fastq_sha256', 64)->nullable();
            $table->json('metadata')->nullable();
            // product_type, claimed_species, batch_id, etc.
            $table->string('screening_result')->nullable();
            // CLEAR / ALERT / REVIEW / RUN_FAIL / CONTROL_CONTAMINATION
            $table->boolean('out_of_panel_flag')->default(false);
            $table->json('out_of_panel_details')->nullable();
            $table->json('species_results')->nullable();
            $table->json('evidence_summary')->nullable();
            $table->integer('total_reads')->nullable();
            $table->integer('classified_reads')->nullable();
            $table->timestamps();
            $table->softDeletes();
            $table->index('run_id');
            $table->index('role');
            $table->index('screening_result');
            $table->unique(['run_id', 'sample_id']);
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('samples');
    }
};
