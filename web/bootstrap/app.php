<?php

use Illuminate\Foundation\Application;
use Illuminate\Foundation\Configuration\Exceptions;
use Illuminate\Foundation\Configuration\Middleware;
use Illuminate\Http\Request;

$app = Application::configure(basePath: dirname(__DIR__))
    ->withRouting(
        web: __DIR__.'/../routes/web.php',
        commands: __DIR__.'/../routes/console.php',
        health: '/up',
    )
    ->withMiddleware(function (Middleware $middleware): void {
        //
    })
    ->withExceptions(function (Exceptions $exceptions): void {
        $exceptions->shouldRenderJsonWhen(
            fn (Request $request) => $request->is('api/*'),
        );
    })->create();

$rootEnvPath = dirname(__DIR__, 2);

if (file_exists($rootEnvPath.'/.env')) {
    $app->useEnvironmentPath($rootEnvPath);

    if (is_dir($rootEnvPath.'/storage')) {
        $app->useStoragePath($rootEnvPath.'/storage');
    }
}

return $app;
