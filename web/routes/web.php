<?php

use App\Http\Controllers\CalibrationController;
use App\Http\Controllers\DashboardController;
use App\Http\Controllers\DatabaseController;
use App\Http\Controllers\ProfileController;
use App\Http\Controllers\ReportController;
use App\Http\Controllers\RunController;
use App\Http\Controllers\SampleController;
use App\Http\Controllers\UploadController;
use Illuminate\Support\Facades\Route;

Route::view('/', 'welcome')->name('welcome');
Route::view('/features', 'features')->name('features');

Route::middleware(['auth', 'verified'])->group(function () {
    // Dashboard
    Route::get('/dashboard', [DashboardController::class, 'index'])->name('dashboard');

    // Upload workflow
    Route::get('/upload', [UploadController::class, 'create'])->name('upload.create');
    Route::post('/upload', [UploadController::class, 'store'])->name('upload.store');

    // Runs
    Route::get('/runs', [RunController::class, 'index'])->name('runs.index');
    Route::get('/runs/{run}', [RunController::class, 'show'])->name('runs.show');
    Route::get('/runs/{run}/qc', [RunController::class, 'qc'])->name('runs.qc');
    Route::delete('/runs/{run}', [RunController::class, 'destroy'])->name('runs.destroy');

    // Samples
    Route::get('/samples/{sample}', [SampleController::class, 'show'])->name('samples.show');
    Route::get('/samples/{sample}/json', [SampleController::class, 'json'])->name('samples.json');

    // Reports / Exports
    Route::get('/samples/{sample}/report/json', [ReportController::class, 'sampleJson'])->name('reports.sample.json');
    Route::get('/samples/{sample}/report/tsv', [ReportController::class, 'sampleTsv'])->name('reports.sample.tsv');
    Route::get('/samples/{sample}/report/pdf', [ReportController::class, 'samplePdf'])->name('reports.sample.pdf');
    Route::get('/runs/{run}/report/zip', [ReportController::class, 'runZip'])->name('reports.run.zip');

    // Database manager
    Route::get('/databases', [DatabaseController::class, 'index'])->name('databases.index');
    Route::get('/databases/create', [DatabaseController::class, 'create'])->name('databases.create');
    Route::post('/databases', [DatabaseController::class, 'store'])->name('databases.store');
    Route::get('/databases/{database}', [DatabaseController::class, 'show'])->name('databases.show');
    Route::post('/databases/{database}/activate', [DatabaseController::class, 'activate'])->name('databases.activate');

    // Calibration manager
    Route::get('/calibrations', [CalibrationController::class, 'index'])->name('calibrations.index');
    Route::get('/calibrations/create', [CalibrationController::class, 'create'])->name('calibrations.create');
    Route::post('/calibrations', [CalibrationController::class, 'store'])->name('calibrations.store');
    Route::get('/calibrations/{calibration}', [CalibrationController::class, 'show'])->name('calibrations.show');
    Route::post('/calibrations/{calibration}/activate', [CalibrationController::class, 'activate'])->name('calibrations.activate');

    // Profile (Breeze)
    Route::get('/profile', [ProfileController::class, 'edit'])->name('profile.edit');
    Route::patch('/profile', [ProfileController::class, 'update'])->name('profile.update');
    Route::delete('/profile', [ProfileController::class, 'destroy'])->name('profile.destroy');
});

require __DIR__.'/auth.php';
