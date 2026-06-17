<?php

namespace Tests\Feature;

use App\Models\ReferenceDatabase;
use App\Models\Run;
use App\Models\Sample;
use App\Models\User;
use App\Services\SpeciesIDEngine;
use Illuminate\Foundation\Testing\RefreshDatabase;
use Illuminate\Http\UploadedFile;
use Illuminate\Support\Facades\Storage;
use ReflectionMethod;
use Tests\TestCase;

class SpeciesIdWorkflowTest extends TestCase
{
    use RefreshDatabase;

    public function test_models_keep_integer_ids_and_public_uuids(): void
    {
        $user = User::factory()->create();
        $database = $this->referenceDatabase();

        $run = Run::create([
            'user_id' => $user->id,
            'reference_database_id' => $database->id,
            'status' => 'pending',
            'label' => 'UUID check',
            'marker_panel' => ['16S'],
        ]);

        $sample = Sample::create([
            'run_id' => $run->id,
            'sample_id' => 'sample_1',
            'role' => 'sample',
            'fastq_path' => 'runs/test/sample.fq',
        ]);

        $this->assertIsInt($run->id);
        $this->assertNotEmpty($run->uuid);
        $this->assertSame($run->id, $sample->run_id);
        $this->assertNotEmpty($sample->uuid);
    }

    public function test_users_cannot_view_another_users_run(): void
    {
        $owner = User::factory()->create();
        $other = User::factory()->create();
        $database = $this->referenceDatabase();

        $run = Run::create([
            'user_id' => $owner->id,
            'reference_database_id' => $database->id,
            'status' => 'completed',
            'label' => 'Private run',
            'marker_panel' => ['16S'],
        ]);

        $this->actingAs($other)->get(route('runs.show', $run))->assertForbidden();
    }

    public function test_lab_manager_can_view_any_run(): void
    {
        $owner = User::factory()->create();
        $manager = User::factory()->create(['role' => 'lab_manager']);
        $database = $this->referenceDatabase();

        $run = Run::create([
            'user_id' => $owner->id,
            'reference_database_id' => $database->id,
            'status' => 'completed',
            'label' => 'Shared run',
            'marker_panel' => ['16S'],
        ]);

        $this->actingAs($manager)->get(route('runs.show', $run))->assertOk();
    }

    public function test_upload_requires_at_least_one_test_sample(): void
    {
        Storage::fake('fastq');
        $user = User::factory()->create();
        $database = $this->referenceDatabase(['is_active' => true]);

        $response = $this->actingAs($user)->post(route('upload.store'), [
            'reference_database_id' => $database->id,
            'marker_panel' => ['16S'],
            'samples' => [
                [
                    'role' => 'negative_control',
                    'fastq' => UploadedFile::fake()->create('ntc.fq', 1, 'text/plain'),
                ],
            ],
        ]);

        $response->assertSessionHasErrors('samples');
    }

    public function test_engine_manifest_omits_null_sample_labels(): void
    {
        Storage::fake('fastq');
        $user = User::factory()->create();
        $database = $this->referenceDatabase();

        $run = Run::create([
            'user_id' => $user->id,
            'reference_database_id' => $database->id,
            'status' => 'pending',
            'label' => 'Unlabeled sample run',
            'marker_panel' => ['16S'],
            'analysis_params' => [
                'detection_threshold' => '0.001',
                'n_bootstrap' => '0',
                'control_adjustment' => '1',
            ],
            'run_metadata' => [
                'pcr_cycles' => '35',
            ],
        ]);

        Sample::create([
            'run_id' => $run->id,
            'sample_id' => 'sample_1',
            'role' => 'sample',
            'label' => null,
            'fastq_path' => 'runs/test/sample.fq',
            'metadata' => [
                'product_type' => null,
                'claimed_species' => ['beef', null, ''],
                'notes' => '',
            ],
        ]);

        $manifest = app(SpeciesIDEngine::class)->buildManifest($run);

        $this->assertArrayNotHasKey('label', $manifest['samples'][0]);
        $this->assertArrayNotHasKey('product_type', $manifest['samples'][0]['metadata']);
        $this->assertArrayNotHasKey('notes', $manifest['samples'][0]['metadata']);
        $this->assertSame(['beef'], $manifest['samples'][0]['metadata']['claimed_species']);
        $this->assertSame('sample_1', $manifest['samples'][0]['sample_id']);
        $this->assertSame(0.001, $manifest['analysis_params']['detection_threshold']);
        $this->assertSame(0, $manifest['analysis_params']['n_bootstrap']);
        $this->assertTrue($manifest['analysis_params']['control_adjustment']);
        $this->assertSame(35, $manifest['run_metadata']['pcr_cycles']);
    }

    public function test_engine_rejects_schema_invalid_results(): void
    {
        $engine = app(SpeciesIDEngine::class);
        $method = new ReflectionMethod($engine, 'validateResult');
        $method->setAccessible(true);

        $this->expectException(\UnexpectedValueException::class);
        $method->invoke($engine, ['schema_version' => '1.0.0']);
    }

    public function test_web_and_schema_text_avoid_legacy_detection_limit_wording(): void
    {
        $paths = [
            base_path('../schema/result_v1.json'),
            resource_path('views/reports/sample_pdf.blade.php'),
            resource_path('views/samples/show.blade.php'),
        ];

        foreach ($paths as $path) {
            $text = file_get_contents($path);
            $this->assertStringNotContainsString('Detection limit', $text, $path);
            $this->assertStringNotContainsString('detection_limit_estimate', $text, $path);
            $this->assertStringNotContainsString('detection limits', $text, $path);
        }
    }

    private function referenceDatabase(array $overrides = []): ReferenceDatabase
    {
        return ReferenceDatabase::create(array_merge([
            'name' => 'Test DB',
            'version' => 'v1',
            'file_path' => 'databases/test.db',
            'index_path' => 'indexes/test.idx',
            'sha256_hash' => str_repeat('a', 64),
            'marker_panel' => ['16S'],
            'species_list' => [],
            'n_species' => 0,
            'n_markers' => 1,
            'is_active' => false,
        ], $overrides));
    }
}
