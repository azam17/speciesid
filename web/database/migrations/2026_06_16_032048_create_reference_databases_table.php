<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('reference_databases', function (Blueprint $table) {
            $table->id();
            $table->uuid()->unique();
            $table->string('name');
            $table->string('version');
            $table->string('file_path');
            $table->string('index_path');
            $table->string('sha256_hash', 64);
            $table->json('marker_panel');
            $table->json('species_list');
            $table->integer('n_species');
            $table->integer('n_markers');
            $table->json('resolvability_matrix')->nullable();
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
        Schema::dropIfExists('reference_databases');
    }
};
