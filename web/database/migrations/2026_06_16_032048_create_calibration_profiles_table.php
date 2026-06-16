<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('calibration_profiles', function (Blueprint $table) {
            $table->id();
            $table->uuid()->unique();
            $table->string('name');
            $table->string('version');
            $table->string('file_path');
            $table->string('sha256_hash', 64);
            $table->foreignId('reference_database_id')->constrained()->cascadeOnDelete();
            $table->double('d_mu')->comment('LogNormal prior mean on DNA yield');
            $table->double('d_sigma')->comment('LogNormal prior stddev on DNA yield');
            $table->double('b_mu')->comment('LogNormal prior mean on PCR bias');
            $table->double('b_sigma')->comment('LogNormal prior stddev on PCR bias');
            $table->integer('n_calibration_samples');
            $table->json('residuals')->nullable();
            $table->boolean('is_locked')->default(false);
            $table->boolean('is_active')->default(false);
            $table->text('notes')->nullable();
            $table->timestamps();
            $table->softDeletes();
            $table->index('sha256_hash');
            $table->index('is_active');
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('calibration_profiles');
    }
};
