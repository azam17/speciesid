<?php

namespace App\Providers;

use App\Models\CalibrationProfile;
use App\Models\ReferenceDatabase;
use App\Models\Report;
use App\Models\Run;
use App\Models\Sample;
use App\Policies\CalibrationProfilePolicy;
use App\Policies\ReferenceDatabasePolicy;
use App\Policies\ReportPolicy;
use App\Policies\RunPolicy;
use App\Policies\SamplePolicy;
use Illuminate\Support\Facades\Gate;
use Illuminate\Support\ServiceProvider;

class AppServiceProvider extends ServiceProvider
{
    /**
     * Register any application services.
     */
    public function register(): void
    {
        //
    }

    /**
     * Bootstrap any application services.
     */
    public function boot(): void
    {
        Gate::policy(Run::class, RunPolicy::class);
        Gate::policy(Sample::class, SamplePolicy::class);
        Gate::policy(Report::class, ReportPolicy::class);
        Gate::policy(ReferenceDatabase::class, ReferenceDatabasePolicy::class);
        Gate::policy(CalibrationProfile::class, CalibrationProfilePolicy::class);
    }
}
